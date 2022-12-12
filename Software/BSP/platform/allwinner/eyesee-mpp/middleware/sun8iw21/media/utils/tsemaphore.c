//#include <CDX_LogNDebug.h>
#define LOG_TAG "SEM"
#include <utils/plat_log.h>

#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include "tsemaphore.h"

/** Initializes the semaphore at a given value
 *
 * @param tsem the semaphore to initialize
 * @param val the initial value of the semaphore
 *
 */
int cdx_sem_init(cdx_sem_t* tsem, unsigned int val)
{
	int i;
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
	i = pthread_cond_init(&tsem->condition, &condAttr);
	if (i!=0)
		return -1;

	i = pthread_mutex_init(&tsem->mutex, NULL);
	if (i!=0)
		return -1;

	tsem->semval = val;

	return 0;
}

/** Destroy the semaphore
 *
 * @param tsem the semaphore to destroy
 */
void cdx_sem_deinit(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);
	pthread_cond_destroy(&tsem->condition);
	pthread_mutex_unlock(&tsem->mutex);

	pthread_mutex_destroy(&tsem->mutex);
}

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero.
 *
 * @param tsem the semaphore to decrease
 */
void cdx_sem_down(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);

	alogv("semdown:%p val:%d",tsem,tsem->semval);
	while (tsem->semval == 0)
	{
		alogv("semdown wait:%p val:%d",tsem,tsem->semval);
		pthread_cond_wait(&tsem->condition, &tsem->mutex);
		alogv("semdown wait end:%p val:%d",tsem,tsem->semval);
	}

	tsem->semval--;
	pthread_mutex_unlock(&tsem->mutex);
}

int cdx_sem_down_timedwait(cdx_sem_t* tsem, unsigned int timeout)
{
    int ret = 0;
    pthread_mutex_lock(&tsem->mutex);

    alogv("semdown:%p val:%d",tsem,tsem->semval);
    while(tsem->semval == 0)
    {
        alogv("semdown wait:%p val:%d",tsem, tsem->semval);
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int relative_sec = timeout/1000;
        int relative_nsec = (timeout%1000)*1000000;
        ts.tv_sec += relative_sec;
        ts.tv_nsec += relative_nsec;
        ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
        ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
        ret = pthread_cond_timedwait(&tsem->condition, &tsem->mutex, &ts);
        if(ETIMEDOUT == ret)
        {
            //alogd("pthread cond timeout np timeout[%d]", ret);
            break;
        }
        else if(0 == ret)
        {
        }
        else
        {
            aloge("fatal error! pthread cond timedwait[%d]", ret);
        }
        alogv("semdown wait end:%p val:%d",tsem,tsem->semval);
    }
    if(tsem->semval > 0)
    {
        if(ret != 0)
        {
            if(ETIMEDOUT == ret)
            {
                aloge("fatal error! semval[%d]>0 when ETIMEDOUT[%d], check code!", tsem->semval, ret);
            }
            else
            {
                aloge("fatal error! semval[%d]>0 when ret[%d], check code!", tsem->semval, ret);
            }
            ret = 0;
        }
        tsem->semval--;
    }
    pthread_mutex_unlock(&tsem->mutex);
    return ret;
}

/** Increases the value of the semaphore
 *
 * @param tsem the semaphore to increase
 */
void cdx_sem_up(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);

	tsem->semval++;
	alogv("semup signal:%p val:%d",tsem,tsem->semval);
	pthread_cond_signal(&tsem->condition);

	pthread_mutex_unlock(&tsem->mutex);
}

void cdx_sem_wait_unique(cdx_sem_t* tsem)
{
    int ret = 0;
    pthread_mutex_lock(&tsem->mutex);
    while(0 == tsem->semval)
    {
        ret = pthread_cond_wait(&tsem->condition, &tsem->mutex);
        if(ret != 0)
        {
            aloge("fatal error! pthread cond wait fail[%d]", ret);
        }
    }
    pthread_mutex_unlock(&tsem->mutex);
}

/*
 * @timeout unit:ms
 */
int cdx_sem_timedwait_unique(cdx_sem_t* tsem, unsigned int timeout)
{
    int ret = 0;
    pthread_mutex_lock(&tsem->mutex);
    while(0 == tsem->semval)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int relative_sec = timeout/1000;
        int relative_nsec = (timeout%1000)*1000000;
        ts.tv_sec += relative_sec;
        ts.tv_nsec += relative_nsec;
        ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
        ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
        ret = pthread_cond_timedwait(&tsem->condition, &tsem->mutex, &ts);
        if(ETIMEDOUT == ret)
        {
            //alogd("Be careful! pthread cond timedwait timeout[%d]", ret);
            break;
        }
        else if(0 == ret)
        {
        }
        else
        {
            aloge("fatal error! pthread cond timedwait[%d]", ret);
        }
    }
    pthread_mutex_unlock(&tsem->mutex);
    return ret;
}

void cdx_sem_up_unique(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);
    
    if(0 == tsem->semval)
    {
        tsem->semval++;
	    pthread_cond_signal(&tsem->condition);
    }

	pthread_mutex_unlock(&tsem->mutex);
}

/** Reset the value of the semaphore
 *
 * @param tsem the semaphore to reset
 */
void cdx_sem_reset(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);

	tsem->semval=0;

	pthread_mutex_unlock(&tsem->mutex);
}

/** Wait on the condition.
 *
 * @param tsem the semaphore to wait
 */
void cdx_sem_wait(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);

	pthread_cond_wait(&tsem->condition, &tsem->mutex);

	pthread_mutex_unlock(&tsem->mutex);
}

/** Wait on the condition.
 *
 * @param tsem the semaphore to wait with time
 */
int cdx_sem_timewait(cdx_sem_t* tsem, unsigned int msec)
{
    int ret;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int relative_sec = msec/1000;
    int relative_nsec = (msec%1000)*1000000;
    ts.tv_sec += relative_sec;
    ts.tv_nsec += relative_nsec;
    ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
    ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);

    pthread_mutex_lock(&tsem->mutex);
    ret = pthread_cond_timedwait(&tsem->condition, &tsem->mutex, &ts);
    pthread_mutex_unlock(&tsem->mutex);
    return ret;
}

/** Signal the condition,if waiting
 *
 * @param tsem the semaphore to signal
 */
void cdx_sem_signal(cdx_sem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);

	pthread_cond_signal(&tsem->condition);

	pthread_mutex_unlock(&tsem->mutex);
}

unsigned int cdx_sem_get_val(cdx_sem_t* tsem)
{
    pthread_mutex_lock(&tsem->mutex);
    unsigned int val = tsem->semval;
    pthread_mutex_unlock(&tsem->mutex);
    return val;
}

