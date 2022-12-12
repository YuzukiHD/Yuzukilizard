/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : cedarx_avs_counter.c
* Version: V1.0
* By     : eric_wang
* Date   : 2015-9-29
* Description:
    add status of idle, pause, executing.
    when pause, system time pause.

    system clock        : linux kernel clock.
    custom system clock : base on system clock, but decrease pause duration.
    cedarx clock        : base on custom system clock, start from 0. add vps support, provide to ClockComponent.
    media clock         : base on cedarx clock, start from video/audio frame pts. ClockComponent maintain it to provide to other component.

    avsCounter maintain cedarx clock, and provide it to ClockComponent.
********************************************************************************
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "AvsCounter"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CDX_ErrorType.h>
#include <cedarx_avs_counter.h>

static long long avscounter_get_system_time()
{
	long long curr;
#if 0
	struct timeval now;
	gettimeofday(&now, NULL);
	//LOGD("now.tv_sec:%lld, %lld", (long long)now.tv_sec,(long long)now.tv_usec);
	curr = (long long)now.tv_sec * 1000000 + now.tv_usec;
#elif 1
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
#else
	avs_counter_get_time_us(&curr);
#endif

	return curr;
}

static void avscounter_reset(struct CedarxAvscounterContext *context)
{
    pthread_mutex_lock(&context->mutex);
	context->mSystemPauseBaseTime = context->system_base_time = context->sample_time = avscounter_get_system_time();
	context->base_time = 0;
	context->adjust_ratio = 0;
    context->mSystemPauseDuration = 0;

    context->mStatus = AvsCounter_StateIdle;
    pthread_mutex_unlock(&context->mutex);
}

/*******************************************************************************
Function name: avscounter_get_time_l
Description: 
    get custom system time: map from system clock, but decrease pause duration.
    get cedarx time: calculate by custom system time and adjust_ratio.
Parameters: 
    curr: cedarx time;
    sample_time: custom system time.
Return: 
    
Time: 2015/9/29
*******************************************************************************/
static void avscounter_get_time_l(struct CedarxAvscounterContext *context, long long *curr, long long *sample_time)
{
    long long system_time;
    if(AvsCounter_StateExecuting == context->mStatus)
    {
        system_time = avscounter_get_system_time() - context->mSystemPauseDuration;
    }
    else
    {
        system_time = context->mSystemPauseBaseTime - context->mSystemPauseDuration;
    }
    *curr = (system_time - context->sample_time) * (100 - context->adjust_ratio) / 100 + context->base_time;
    if(sample_time != NULL) 
    {
        *sample_time = system_time;
    }
}

/*******************************************************************************
Function name: avscounter_get_time
Description: 
    get cedarx clock time.
Parameters: 
    
Return: 
    
Time: 2015/10/29
*******************************************************************************/
static void avscounter_get_time(struct CedarxAvscounterContext *context, long long *curr)
{
	pthread_mutex_lock(&context->mutex);
	avscounter_get_time_l(context, curr, NULL);
	pthread_mutex_unlock(&context->mutex);
}

static void avscounter_get_time_diff(struct CedarxAvscounterContext *context, long long *diff)
{
	long long curr_time,sys_time;
	pthread_mutex_lock(&context->mutex);
	avscounter_get_time_l(context, &curr_time, &sys_time);
	*diff = curr_time - (sys_time - context->system_base_time);
	pthread_mutex_unlock(&context->mutex);
}

static void avscounter_adjust(struct CedarxAvscounterContext *context, int val)
{
	long long curr;

	pthread_mutex_lock(&context->mutex);
    if(context->adjust_ratio!=val)
    {
    	avscounter_get_time_l(context, &curr, &context->sample_time);
    	context->base_time = curr;
    	context->adjust_ratio = val;
    }
	pthread_mutex_unlock(&context->mutex);
}

static int avscounter_start(struct CedarxAvscounterContext *context)
{
    int ret = CDX_OK;
    pthread_mutex_lock(&context->mutex);
    if(AvsCounter_StateIdle == context->mStatus || AvsCounter_StatePause == context->mStatus)
    {
        long long nDuration = avscounter_get_system_time() - context->mSystemPauseBaseTime;
        context->mSystemPauseDuration += nDuration;
        context->mStatus = AvsCounter_StateExecuting;
        alogd("(f:%s, l:%d) Avscounter status [%s]->[run], pauseDuration[%lld][%lld]ms", __FUNCTION__, __LINE__, 
            context->mStatus==AvsCounter_StateIdle?"idle":"pause", nDuration/1000, context->mSystemPauseDuration/1000);
    }
    else if(AvsCounter_StateExecuting == context->mStatus)
    {
        alogd("(f:%s, l:%d) Avscounter already run", __FUNCTION__, __LINE__);
    }
    else
    {
        aloge("(f:%s, l:%d) fatal error! wrong status[%d]", __FUNCTION__, __LINE__, context->mStatus);
    }
	pthread_mutex_unlock(&context->mutex);
    return ret;
}

static int avscounter_pause(struct CedarxAvscounterContext *context)
{
    int ret = CDX_OK;
    pthread_mutex_lock(&context->mutex);
    if(AvsCounter_StateExecuting == context->mStatus)
    {
        alogd("(f:%s, l:%d) Avscounter status run->pause", __FUNCTION__, __LINE__);
        context->mSystemPauseBaseTime = avscounter_get_system_time();
        context->mStatus = AvsCounter_StatePause;
    }
    else if(AvsCounter_StatePause == context->mStatus)
    {
        alogd("(f:%s, l:%d) Avscounter already pause", __FUNCTION__, __LINE__);
    }
    else if(AvsCounter_StateIdle == context->mStatus)
    {
        alogd("(f:%s, l:%d) Avscounter status idle->pause", __FUNCTION__, __LINE__);
        context->mStatus = AvsCounter_StatePause;
    }
    else
    {
        aloge("(f:%s, l:%d) fatal error! wrong status[%d]", __FUNCTION__, __LINE__, context->mStatus);
    }
    
	pthread_mutex_unlock(&context->mutex);
    return ret;
}

CedarxAvscounterContext *cedarx_avs_counter_request()
{
	CedarxAvscounterContext *context;

	context = (CedarxAvscounterContext *)malloc(sizeof(CedarxAvscounterContext));
	memset(context, 0, sizeof(CedarxAvscounterContext));

	context->reset = avscounter_reset;
	context->get_time = avscounter_get_time;
	context->get_time_diff = avscounter_get_time_diff;
	context->adjust = avscounter_adjust;

    context->start = avscounter_start;
    context->pause = avscounter_pause;

	pthread_mutex_init(&context->mutex, NULL);

    context->mStatus = AvsCounter_StateIdle;
	avscounter_reset(context);

	return context;
}

int cedarx_avs_counter_release(CedarxAvscounterContext *context)
{
	if(context != NULL) {
		pthread_mutex_destroy(&context->mutex);
		free(context);
		//context = NULL;
	}

	return 0;
}

