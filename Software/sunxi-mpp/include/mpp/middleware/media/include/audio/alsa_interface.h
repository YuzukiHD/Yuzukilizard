#ifndef __ALSA_COMMON_H__
#define __ALSA_COMMON_H__

#include <alsa/asoundlib.h>
#include <string.h>

#define AUDIO_VOLUME_MIN    (0)
#define AUDIO_VOLUME_MAX    (31)
#define AUDIO_LEFT_INPUT_MIC1 "Left Input Mixer MIC1 Boost"
#define AUDIO_LEFT_INPUT_MIC2 "Left Input Mixer MIC2 Boost"
#define AUDIO_RIGHT_INPUT_MIC1 "Right Input Mixer MIC1 Boost"
#define AUDIO_RIGHT_INPUT_MIC2 "Right Input Mixer MIC2 Boost"
#define HEAD_PHONE "Headphone"
#define HEAD_PHONE_VOL "Headphone volume"
#define DIGITAL_VOL "digital volume"
#define HPOUT_VOL "HPOUT volume"

#if 0
// V5 controls name
//for play
#define AUDIO_LINEOUT_VOL       "lineout volume"
#define AUDIO_LINEOUT_SWITCH    "Lineout"
#define AUDIO_DACL_MIXER        "DACL Mixer AIF1DA0L"
#define AUDIO_DACR_MIXER        "DACR Mixer AIF1DA0R"
#define AUDIO_LOUTPUT_DACL_MIXER     "Left Output Mixer DACL"
#define AUDIO_LOUTPUT_DACR_MIXER     "Left Output Mixer DACR"
#define AUDIO_ROUTPUT_DACL_MIXER     "Right Output Mixer DACL"
#define AUDIO_ROUTPUT_DACR_MIXER     "Right Output Mixer DACR"
#define AUDIO_PA_SWITCH         "Speaker PA shutdown pin high level"

//for cap
#define AUDIO_AD0L_MIXER        "AIF1 AD0L Mixer ADCL"
#define AUDIO_AD0R_MIXER        "AIF1 AD0R Mixer ADCR"
#define AUDIO_LADC_MIC1_SWITCH  "LADC input Mixer MIC1 boost"
#define AUDIO_RADC_MIC1_SWITCH  "RADC input Mixer MIC1 boost"
#define AUDIO_LINEINL_SWITCH    "LADC input Mixer LINEINL"
#define AUDIO_LINEINR_SWITCH    "RADC input Mixer LINEINR"

//for cap gain and vol
#define AUDIO_MIC1_GAIN         "MIC1 boost AMP gain control"
#define AUDIO_ADC_GAIN          "ADC input gain control"
#define AUDIO_ADC_VOLUME        "ADC volume"

//for play gain and vol
#define AUDIO_DAC_VOLUME        "DAC volume"
#define AUDIO_DAC_MIXER_GAIN    "DAC mixer gain"

//for lineout L/R source select to increase vol
#define AUDIO_LINEOUTL_MUX      "LINEOUTL Mux"
#define AUDIO_LINEOUTR_MUX      "LINEOUTR Mux"
#endif /* 0 */

#if 0
// for v459
    // for play
#define AUDIO_LINEOUT_VOL       "LINEOUT volume"            // set play volume
#define AUDIO_PA_SWITCH         "External Speaker"           // play switch 
#define AUDIO_LINEOUTL_MUX      "Left LINEOUT Mux"            // perl board:1;ipc prototype board:0

    // for cap
#define AUDIO_LADC_MIC1_SWITCH  "Left Input Mixer MIC1 Boost"   // mic witch
#define AUDIO_AD0L_MIXER        "Left Input Mixer LINEINL"     // line_in switch
#define AUDIO_MIC1_MAIN_GAIN    "MIC1 gain volume"

    // v459 aec to enable cap&play sync mode for aec feature
#define AUDIO_CAP_PLAY_SYNC_MODE "codec trigger substream mode"

#define AUDIO_CODEC_HUB_MODE "codec hub mode"

#define DAUDIo0_HUB_MODE   "sunxi daudio audio hub mode"
#define DAUDIo0_LOOPBACK_EN "sunxi daudio loopback debug"

#define AUDIO_DACDRC_EN   "DACDRC"
#define AUDIO_ADCDRC_EN   "DACDRC"

#define AUDIO_DACHPF_EN   "DACHPF"
#define AUDIO_ADCHPF_EN   "DACHPF"

#endif


// for v853 
// about lineout
#define AUDIO_LINEOUT_SWITCH    "LINEOUT"
#define AUDIO_LINEOUT_MUX       "LINEOUT Output Select"
#define AUDIO_LINEOUT_VOL       "LINEOUT volume"
#define AUDIO_PA_SWITCH         "SPK"

// about mic
#define AUDIO_ADC_MIC1_SWITCH   "MIC1"
#define AUDIO_ADC_MIC2_SWITCH   "MIC2"
#define AUDIO_ADC_MIC1_SELECT   "MIC1 Input Select" 
#define AUDIO_ADC_MIC2_SELECT   "MIC2 Input Select"
#define AUDIO_MIC1_MAIN_GAIN    "MIC1 gain volume"
#define AUDIO_MIC2_MAIN_GAIN    "MIC2 gain volume"

// about linein
#define AUDIO_LINEIN_SWITCH        "LINEIN"
#define AUDIO_LINEIN_L_GAIN        "LINEINL gain volume"
#define AUDIO_LINEIN_R_GAIN        "LINEINR gain volume"

    // v459 aec to enable cap&play sync mode for aec feature
#define AUDIO_CAP_PLAY_SYNC_MODE "rx sync mode"

#define AUDIO_CODEC_HUB_MODE "tx hub mode"

#define DAUDIo0_HUB_MODE   "tx hub mode"
#define DAUDIo0_LOOPBACK_EN "loopback debug"

#define AUDIO_DACDRC_EN   "DACDRC"
#define AUDIO_ADCDRC_EN   "ADCDRC"

#define AUDIO_DACHPF_EN   "DACHPF"
#define AUDIO_ADCHPF_EN   "ADCHPF"






typedef struct PCM_CONFIG_S
{
    snd_pcm_t *handle;
    char cardName[16];

    snd_pcm_format_t format;
    unsigned int chnCnt;
    unsigned int sampleRate;

    snd_pcm_uframes_t bufferSize;
    snd_pcm_uframes_t chunkSize;    //unit: sample number

    unsigned int bitsPerSample;
    unsigned int significantBitsPerSample;
    unsigned int bitsPerFrame;
    unsigned int chunkBytes;    //unit: bytes
    int aec_delay_ms;
    
    int snd_card_id;
    int read_pcm_aec;             // used to indicate aec condition or normal one when read pcm
} PCM_CONFIG_S;

typedef struct AIO_MIXER_S {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    
    int snd_card_id;
} AIO_MIXER_S;

int alsaSetPcmParams(PCM_CONFIG_S *pcmCfg);
int alsaOpenPcm(PCM_CONFIG_S *pcmCfg, const char *card, int pcmFlag);
void alsaClosePcm(PCM_CONFIG_S *pcmCfg, int pcmFlag);
void alsaPreparePcm(PCM_CONFIG_S *pcmCfg);

ssize_t alsaReadPcm(PCM_CONFIG_S *pcmCfg, void *data, size_t rcount);
ssize_t alsaWritePcm(PCM_CONFIG_S *pcmCfg, void *data, size_t wcount);
int alsaDrainPcm(PCM_CONFIG_S *pcmCfg);
int alsaOpenMixer(AIO_MIXER_S *mixer, const char *card);
void alsaCloseMixer(AIO_MIXER_S *mixer);
int alsaMixerSetCapPlaySyncMode(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecHubMode(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetDAudio0HubMode(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetDAudio0LoopBackEn(AIO_MIXER_S *mixer,  int value);

int alsaMixerSetAudioCodecDacDrc(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecAdcDrc(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecDacHpf(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecAdcHpf(AIO_MIXER_S *mixer,  int value);

int alsaMixerSetVolume(AIO_MIXER_S *mixer, int playFlag, long value);
int alsaMixerGetVolume(AIO_MIXER_S *mixer, int playFlag, long *value);
int alsaMixerSetMute(AIO_MIXER_S *mixer, int playFlag, int bEnable);
int alsaMixerGetMute(AIO_MIXER_S *mixer, int playFlag, int *pVolVal);
void updateDebugfsByChnCnt(int pcmFlag, int cnt);

int alsaGetDelay(PCM_CONFIG_S *pcmCfg);
int alsaMixerSetPlayBackPA(AIO_MIXER_S *mixer, int bHighLevel);
int alsaMixerGetPlayBackPA(AIO_MIXER_S *mixer, int *pbHighLevel);

int alsaMixerSetMic2Enable(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetLineInEnable(AIO_MIXER_S *mixer,  int value);
#endif /* __ALSA_COMMON_H__ */
