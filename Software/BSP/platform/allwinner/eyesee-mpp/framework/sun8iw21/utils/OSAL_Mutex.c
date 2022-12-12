/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : OSAL_Mutex.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/02
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "OSAL_Mutex"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "OSAL_Mutex.h"

int OSAL_MutexCreate(void **mutexHandle)
{
    pthread_mutex_t *mutex;

    mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!mutex)
        return -1;

    if (pthread_mutex_init(mutex, NULL) != 0)
        return -1;

    *mutexHandle = (void*)mutex;
    return 0;
}

int OSAL_MutexTerminate(void *mutexHandle)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutexHandle;

    if (mutex == NULL)
        return -1;

    if (pthread_mutex_destroy(mutex) != 0)
        return -1;

    if (mutex)
        free(mutex);
    return 0;
}

int OSAL_MutexLock(void *mutexHandle)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutexHandle;
    int result;

    if (mutex == NULL)
        return -1;

    if (pthread_mutex_lock(mutex) != 0)
        return -1;

    return 0;
}

int OSAL_MutexUnlock(void *mutexHandle)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutexHandle;
    int result;

    if (mutex == NULL)
        return -1;

    if (pthread_mutex_unlock(mutex) != 0)
        return -1;

    return 0;
}

