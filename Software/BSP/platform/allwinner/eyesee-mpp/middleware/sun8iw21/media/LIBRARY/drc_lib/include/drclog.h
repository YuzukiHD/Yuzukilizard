#ifndef DRC_H_
#define DRC_H_

/* dynamic range control parameters */
typedef struct
{
    int sampling_rate;   //sampling rate
    int target_level;    //default value-3
    int max_gain;        //default value:6 max:15dB
    int min_gain;        //default value:-9 min:-15dB
    int attack_time;     //default value:1
    int release_time;    //default value:100
    int noise_threshold; //default value:-45
}drcLog_prms_t;
/*
function :drcinit
init function
input:
    fs:采样率
return :
    dynamic range controller handle
*/
void* drcLog_create(drcLog_prms_t* prms);
/*
function :drcdec
drc function
input:
    handle: dynamic range controlller handle
    xout: 处理数据首地址；
    samplenum: 处理sample数，单通路samplenum = 处理数据总长度/2(short size);双声道samplenum = 处理数据总长度/4;
    nch: 通道数
return :
    无返回值
*/
void drcLog_process(void* handle, short* xout,int samplenum, int nch);

/*
function: drcfree
deinit the heap etc.
input:
    handle: dynamic range controller handle
return value:
    none
*/
void drcLog_destroy(void* handle);

#endif//DRC_H_
