/* tinycap.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include <errno.h>
#include "uv_se_bf.h"
#include "preproc_for_QuanZhi_simplify.h"
#include "wakeup_api.h"
#include "uvoice_wakeup.h"
#include "acmod.h"
#include "uvoice.h"
#include "Resample.h"
#include "remove_click.h"
#define FORMAT_PCM 1
#define IN_BIT 32
#define IN_RATA 32000
#define OUT_BIT 16
#define OUT_RATA 16000
static const int kSamplesPerFrame = 320;
static const int koutSamplesPerFrame = 160;
static const int kplaySamplesPerFrame = 441;
static const int kBlocksForKws = 16;
static int data_len=160*4*16;
static int data_len_big=441*4*16;
ResampleUnit rsp;
static int fs_in=44100,fs_out=16000;
static int lenIn=441*16,lenOut;
static int8_t channel_big=0;
//static int8_t destory_pthr=0;
static const int kOffBatchSize = 2048;
static const int kOnlBatchSize = 1;
#define ACMOD_LEN 453008
//FILE *test_file;
static int kDebug 
= 0;
struct queue {
    char *data;
    int     size;
    int     readp;
    int     writep;
    pthread_mutex_t mutex;
    pthread_cond_t empty;
    pthread_cond_t full;
};
typedef struct
{
    char *ns_buffer;    //use it to store data temporary. can store 160ms 16K-2channel-16bit audio data.
    int mValidDataLen;  //indicate valid data size in ns_buffer.
    unsigned int channels;
    uvoiceCallbackInfo mCallback;
    uvoiceResult mDetectData;
    struct queue *data_queue;

    volatile int8_t destory_pthr;   //flag to tell sendpthread exit
    pthread_t sendpthread;
}uvoiceContext;

struct queue *queue_init(int size)
{
    struct queue *queue = malloc(sizeof(struct queue));

    if(queue == NULL)
        printf("Out of memory in queue_init\n");

    memset(queue, 0, sizeof(struct queue));
    queue->data = malloc(sizeof(char) * (size));
    if(queue->data == NULL)
        printf("Out of memory in queue_init\n");

    queue->size = size;
    queue->readp = queue->writep = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    pthread_cond_init(&queue->empty, &condAttr);
    pthread_cond_init(&queue->full, &condAttr);

    return queue;
}

void queue_exit(struct queue *queue)
{
    if(queue != NULL)
    {
        pthread_mutex_lock(&queue->mutex);
        if(queue->data)
        {
            free(queue->data);
            queue->data = NULL;
            queue->size = queue->readp = queue->writep = 0;
        }
        pthread_mutex_unlock(&queue->mutex);
        pthread_mutex_destroy(&queue->mutex);
        pthread_cond_destroy(&queue->empty);
        pthread_cond_destroy(&queue->full);
        free(queue);
    }
}

void queue_put(struct queue *queue, void *data)
{
    int nextp;

    pthread_mutex_lock(&queue->mutex);

    while((nextp = (queue->writep + data_len) % queue->size) == queue->readp)
    {
        pthread_cond_wait(&queue->full, &queue->mutex);
    }
    memcpy(&queue->data[queue->writep],data,data_len);
    queue->writep = nextp;
    pthread_cond_signal(&queue->empty);
    pthread_mutex_unlock(&queue->mutex);
}


/**
 * @param timeout -1: wait forever; 0:return immediately; >0:timeout,unit:ms
 */
void *queue_prefetch(struct queue *queue, int timeout)
{
    void *data;
    pthread_mutex_lock(&queue->mutex);
    if(queue->readp == queue->writep)
    {
        if(0 == timeout)
        {
            data = NULL;
        }
        else if(timeout < 0)
        {
            while(queue->readp == queue->writep)
            {
                pthread_cond_wait(&queue->empty, &queue->mutex);
            }
            data = &queue->data[queue->readp];
        }
        else
        {
            int ret;
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int relative_sec = timeout/1000;
            int relative_nsec = (timeout%1000)*1000000;
            ts.tv_sec += relative_sec;
            ts.tv_nsec += relative_nsec;
            ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
            ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
            ret = pthread_cond_timedwait(&queue->empty, &queue->mutex, &ts);
            if(ETIMEDOUT == ret)
            {
                //printf("pthread cond timeout np timeout[%d]\n", ret);
                data = NULL;
            }
            else if(0 == ret)
            {
                if(queue->readp == queue->writep)
                {
                    printf("fatal error! wait empty buffer signal up, but no empty buffer!\n");
                    data = NULL;
                }
                else
                {
                    data = &queue->data[queue->readp];
                }
            }
            else
            {
                printf("fatal error! pthread cond timedwait[%d]\n", ret);
                data = NULL;
            }
        }
    }
    else
    {
        data = &queue->data[queue->readp];
    }
    pthread_mutex_unlock(&queue->mutex);

    return data;
}

int queue_flush(struct queue *queue, void *data)
{
    if(NULL == data)
    {
        printf("Be careful! why send null pointer?\n");
        return 0;
    }
    pthread_mutex_lock(&queue->mutex);

    if((char*)data != &queue->data[queue->readp])
    {
        printf("fatal error! why data[%p] != [%p](readp[%d])\n", data, queue->data[queue->readp], queue->readp);
        abort();
    }
    queue->readp = (queue->readp + data_len) % queue->size;
    pthread_cond_signal(&queue->full);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int uvoice_SendData(void *puvoice, char *pData, int nSize)
{
    uvoiceContext *pContext = (uvoiceContext*)puvoice;
    //resample first if sampleRate is not 16K.
    int16_t* loop_buffer=NULL;
    int8_t res=0;
    int size=0;
    static unsigned int wr_peek=0;
    size = koutSamplesPerFrame * kBlocksForKws * channel_big * sizeof(int16_t);
    loop_buffer=malloc(size);


    //store and send data.
    if(pContext->mValidDataLen != 0)
    {
        if(pContext->mValidDataLen >= data_len_big)
        {
            printf("fatal error! validLen[%d]>=[%d]\n", pContext->mValidDataLen, data_len_big);
        }
        if(nSize < (data_len_big - pContext->mValidDataLen))
        {
            memcpy(pContext->ns_buffer+pContext->mValidDataLen, pData, nSize);
            pContext->mValidDataLen += nSize;
	    free(loop_buffer);
            return 0;
        }
        else
        {
            memcpy(pContext->ns_buffer+pContext->mValidDataLen, pData, (data_len_big - pContext->mValidDataLen));
	    wr_peek=data_len_big - pContext->mValidDataLen;
            pContext->mValidDataLen += (data_len_big - pContext->mValidDataLen);
            pData += (data_len_big - pContext->mValidDataLen);
            nSize -= (data_len_big - pContext->mValidDataLen);
        }
    }
    if(pContext->mValidDataLen != 0)
    {
        if(pContext->mValidDataLen!=data_len_big)
        {
            printf("fatal error! validLen[%d]!=[%d]\n", pContext->mValidDataLen, data_len_big);
        }
	res = ResampleProcess(&rsp, (short *)(pContext->ns_buffer),(short *)loop_buffer, lenIn);
        queue_put(pContext->data_queue, loop_buffer);

	memcpy(pContext->ns_buffer, pData+wr_peek, (nSize - wr_peek));
        pContext->mValidDataLen = nSize - wr_peek;
	free(loop_buffer);
	return 0;
    }
    while(nSize >= data_len_big)
    {
	res = ResampleProcess(&rsp, (short *)pData,(short *)loop_buffer, lenIn);
        queue_put(pContext->data_queue, loop_buffer);
        pData += data_len_big;
        nSize -= data_len_big;
    }
    //store left data
    if(nSize > 0)
    {
        memcpy(pContext->ns_buffer, pData, nSize);
        pContext->mValidDataLen = nSize;
    }
    free(loop_buffer);
    return 0;
}

void st_to_mono(int16_t *outptr,int16_t *inptr)
{
    int i=0;
    for(i=0;i<160*16;i++)
    {
        outptr[i]=inptr[i*2+1];
    }
}

static void *nfsdata(void *arg)
{
    char *send_buf=NULL;
    int i = 0;
    int j = 0;
    int32_t code = 0;
    int16_t* kws_buffer = NULL;
    int16_t* kws_buffer_pntr = NULL;
    int16_t* ns_buffer_pntr = NULL;
    uint32_t size;
    void* channel;
    int tick = 0,hold_on_frame = 0;
    int iRet=-1;
    float max_num=0.0;
    int cut_flag=0;
    int cut_count=0;
    int all_cut_flag=0;
    int all_cut_count=0;
    int max_index;
    int count_check=0;
    wakeup_result** results;
    uvoiceContext *pContext=(uvoiceContext*)arg;
    void *hdlSpline;
    int flag;
    hdlSpline = b_spline_create(800);

    void* handle = uvoice_wakeup_handle_init_from_buf(acmod_data,ACMOD_LEN, kOnlBatchSize); // load acmod
    if (NULL == handle) {
        fprintf(stderr, "uvoice_wakeup_handle_init error\n");
        return 0;
    }
    uvoice_wakeup_handle_ctrl(handle, SET_DEBUG, &kDebug);

    unsigned char target_buf[1024]={0};
    FILE* tFile = fopen("/usr/bin/kws.bin", "rb");
    int rc = fread(target_buf, 1, 1024, tFile);
    printf("rc = %d\n", rc);
    if (0 != uvoice_wakeup_load_targets(handle, target_buf, rc)) {
        fprintf(stderr, "uvoice_wakeup_load_targets error\n");
        return 0;
}

    size = koutSamplesPerFrame * kBlocksForKws * 1 * sizeof(int16_t);
    kws_buffer = malloc(size);//32bit to 16bit buffer
    if (!kws_buffer) {
        fprintf(stderr, "Unable to allocate %u bytes\n", size);
        return 0;
    }
    /* NS */
    void* ns = NULL;
    SE_BF_PARAM_CREATE se_param_create = { 0 };
    SE_BF_PARAM_INIT se_param_init = { 0 };


    printf("CTC_KWS version: %s\n", uvoice_wakeup_get_version());
    printf("NS  version: %s\n", se_bf_get_version());


    /* ns */
    se_param_create._num_ch_in = 2;
    se_param_create._b_use_agc = 1;
    ns = se_bf_create(&se_param_create);

    se_param_init._se_type = TYPE_SE_BF_MVDR;
    se_param_init._input_smps = koutSamplesPerFrame * kBlocksForKws;
    se_param_init._b_target_angle_specified = 0;
    se_bf_init(ns, &se_param_init);

    while(1)
    {
	count_check++;
    PROCESS_MESSAGE:
        if(pContext->destory_pthr==1)
        {
            break;
        }

        send_buf=queue_prefetch(pContext->data_queue, 200);
        if(NULL == send_buf)
        {
            goto PROCESS_MESSAGE;
        }
        //se_bf_do(ns, kws_buffer, (int16_t *)send_buf);

        if(pContext->channels!=1)
        {
            st_to_mono(kws_buffer,(int16_t *)send_buf);
        }
        else
        {
            memcpy(kws_buffer,(int16_t *)send_buf,data_len);
        }
        queue_flush(pContext->data_queue, send_buf);
        kws_buffer_pntr = kws_buffer;
        for (i = 0; i < kBlocksForKws; i++) 
        {
        {
	    flag = 0;
	    flag = remove_click(hdlSpline,kws_buffer_pntr, 160, flag);
	    if(flag==1)
	    {
		memset(kws_buffer_pntr, 0, 160*sizeof(short));
	    }
	    /*if(fwrite(kws_buffer_pntr, 1, 160*2, test_file) == 0) {
                 fprintf(stderr,"Error capturing sample\n");
                 break;
            }*/
            iRet = uvoice_wakeup_processWav(handle,(char *)kws_buffer_pntr, kSamplesPerFrame);
            if(iRet>0)
            {
                results = uvoice_wakeup_get_result(handle);
                for (int i = 0; i < iRet; i++) 
                {
                    if(i==0)
                    {
                        max_num=results[0]->prob;
                        max_index=0;
                    }
                    else
                    {
                        if(results[i]->prob>max_num)
                        {
                            max_num = results[i]->prob;
                            max_index =i;
                        }   
                    }
                }
                if(results[max_index]->prob < 0.2 )
                {
                    printf("reject there request!!!\n");
                    iRet=-1;
                }
                else
                {
                    printf("Found <%s>, its prob is:%f,at time:%f,end with with:%f,the command_id is:%d\n",results[max_index]->words, results[max_index]->prob, results[max_index]->offset, results[max_index]->end_offset,results[max_index]->command_id);
                    pContext->mDetectData.eCmd=results[max_index]->command_id;
                    pContext->mCallback.callback(pContext->mCallback.cookie, &pContext->mDetectData);   
                    uvoice_wakeup_clear_result(handle);
                    uvoice_wakeup_handle_clear(handle);
                    iRet=-1;
                    break;
                }
                uvoice_wakeup_handle_reset(handle);
                uvoice_wakeup_clear_result(handle);
                iRet=-1;
            }
        }

        kws_buffer_pntr += koutSamplesPerFrame;
        }

    }
    uvoice_wakeup_handle_fini(handle);
    b_spline_free(hdlSpline);
    free(kws_buffer);
    fclose(tFile);
    //fclose(test_file);
}

void  creat_uvoice_handle(uvoiceContext *pContext)
{
    int err = pthread_create(&pContext->sendpthread, NULL, nfsdata, pContext);
    if (err!=0)
    {
        printf("fatal error! pthread create fail[0x%x][%s]\n", err, strerror(errno));
    }
}

int destructUvoiceInstance(void *puvoice)
{
    int ret;
    uvoiceContext *pContext = (uvoiceContext*)puvoice;
    pContext->destory_pthr=1;
    ResampleUninit(&rsp);
    //ret=pthread_detach(sendpthread);
    ret=pthread_join(pContext->sendpthread,NULL);
    if(ret<0)
    {
        printf("fatal error! destructUvoiceInstance error,ret is:%d\n",ret);
        //return -1;
    }
    queue_exit(pContext->data_queue);
    pContext->data_queue = NULL;
    if(pContext->ns_buffer!=NULL)
    {
        free(pContext->ns_buffer);
        pContext->ns_buffer = NULL;
    }
    free(pContext);
    return 0;
}
int uvoice_RegisterCallback(void *puvoice, uvoiceCallbackInfo *pCallback)
{
    uvoiceContext *pContext = (uvoiceContext*)puvoice;
    pContext->mCallback = *pCallback;
    return 0;
}
void* constructUvoiceInstance(PcmConfig *pConfig)
{
    int all_buffers_size=32000*2*16*3;
    if(pConfig->mSampleRate != 16000 || pConfig->mSampleBitsPerChannel != 16)
    {
	if(pConfig->mSampleRate == 44100)
	{
		lenOut = ResampleInit(&rsp, fs_in, fs_out, lenIn);
    		printf("the lenOut is:%d\n",lenOut);		
	}
        printf("there is no 16000 rate,16bit campate!!!!\n");
    }
    else
    {
        printf("begin to do!!!\n");
    }
    channel_big=pConfig->mChannels;
    if(pConfig->mChannels==1)
    {
	data_len=160*4*16;
	data_len_big=441*4*16;
        data_len=data_len/2;
	data_len_big=data_len_big/2;
    }
    /*test_file = fopen("/mnt/extsd/jxm111.pcm", "wb");
    if (test_file==NULL) {
            printf("there is error to open the file!!!!\n");
    }*/

    uvoiceContext *pContext = (uvoiceContext*)malloc(sizeof(uvoiceContext));
    memset(pContext, 0, sizeof(uvoiceContext));
    pContext->channels =pConfig->mChannels;
    pContext->ns_buffer = (char *)malloc(kplaySamplesPerFrame * kBlocksForKws * pConfig->mChannels * sizeof(int16_t));
    pContext->data_queue = queue_init(all_buffers_size);
    if(pContext->data_queue==NULL)
    {
        printf("uvoice_init error!!!!\n");
    }
    creat_uvoice_handle(pContext);

    return (void*)pContext;
}
