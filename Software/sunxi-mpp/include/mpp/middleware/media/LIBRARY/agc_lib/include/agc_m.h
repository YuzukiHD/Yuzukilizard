#ifndef _AGC_M_H_
#define _AGC_M_H_

#include <stdio.h>
#include <math.h>

#define MAX_SAMPLE_LEN 
#define MAX_GAIN_LEVEL 5
#define FILTER_CNT 6
#define MAX_SAMPLE_DATA (32767)
#define MIN_SAMPLE_DATA (-32767)

typedef struct agc_m_handle
{
    float fSample_rate;         //iSample_rate : 8, 16, 32, 44.1, 48,
    int iSample_len;            //default 10ms:  the sample number processed
    int iBytePerSample;         //bps: 2,- short
    int iGain_level;            //gain_level : 0,1,2
    int iChannel;               //channel   mono:1   stereo:2
    int iReserverd;
    float ffilter[FILTER_CNT];  // filter;
    float fEnergy_Level;

}my_agc_handle;

int agc_m_init(my_agc_handle *ins_agc_handle,
    float sample_rate,
    int sample_len,
    int bytePerSample,
    int gain_level,
    int channel);

int agc_m_process(my_agc_handle *ins_agc_handle, short *sInbuff, short *sOutbuff, int iLen);
#endif  /* _AGC_M_H_ */

