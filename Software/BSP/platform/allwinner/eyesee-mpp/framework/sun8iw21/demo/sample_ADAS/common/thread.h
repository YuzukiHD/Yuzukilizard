#pragma once

#include <pthread.h>
#include <iostream>
#include <sys/prctl.h>

#define ThreadCreate(THREAD_ID, ATTR, FUNC, ARG) \
do { \
    pthread_attr_t *attrp = ATTR; \
    if (attrp == NULL) { \
        pthread_attr_t attr; \
        pthread_attr_init(&attr); \
        attrp = &attr; \
    } \
    pthread_attr_setdetachstate(attrp, PTHREAD_CREATE_DETACHED); \
    int ret = pthread_create(THREAD_ID, attrp, FUNC, ARG); \
    if (ret != 0) { \
        std::cout << "create thread failed" << std::endl; \
    } \
} while(0)
