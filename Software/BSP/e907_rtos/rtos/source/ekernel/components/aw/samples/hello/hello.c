/*
 * ===========================================================================================
 *
 *       Filename:  hello.c
 *
 *    Description:  just test hello world compile in kbuild.
 *
 *        Version:  Melis3.0
 *         Create:  2019-12-23 18:52:00
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  eric_wang@allwinnertech.com
 *   Organization:  PDC-PD5
 *  Last Modified:  2019-12-23 19:00:00
 *
 * ===========================================================================================
 */
#include <stdio.h>

#include <log.h>
#include <finsh.h>

//#include <osal_time.h>
#include <unistd.h>
#include <pthread.h>

#include <clock_time.h>

int func_multiply(int param1, int param2)
{
    int a = 0;
    a = param1 * param2;
    __log("multiply=%d", a);
    return a;
}

int calculate(int a, int b)
{
    return func_multiply(a * 2, b * 3);
}

typedef struct 
{
    volatile int counter;
} hello_atomic_t;

static int CdxAtomicSet(hello_atomic_t *ref, int val)
{
    return __sync_lock_test_and_set(&ref->counter, val);
}

//cast between incompatible function types from 'int (*)(void)' to 'int (*)(int,  const char **)
static int test_hello(int argc, const char **argv)
{
    int result = 0;
    int i;
    unsigned int timeout = 2000; //ms
    for (i = 0; i < argc; i++)
    {
        printf("argv[%d]=[%s]\n", i, argv[i]);
        if(1==i)
        {
            timeout = (unsigned int)strtol(argv[i], NULL, 0);
        }
    }
    
    printf("hello, world! timeout=[%d]ms\n", timeout);
    __log("hello, world, d");
    __err("hello, world, e");
    //sleep(1);
    //__wrn("hello, world, w");
    //msleep(1000);
    __inf("hello, world, i");
    //usleep(1000 * 1000);
    __log("hello, world, m");


    int ret = 0;
    pthread_cond_t 	testCondition;
	pthread_mutex_t testLock;
    
    ret = pthread_cond_init(&testCondition, NULL);
    if (ret!=0)
    {
        printf("fatal error! pthread cond init ret[%d]\n", ret);
    }
    ret = pthread_mutex_init(&testLock, NULL);
    if (ret!=0)
    {
        printf("fatal error! pthread mutex init ret[%d]\n", ret);
    }
    
    pthread_mutex_lock(&testLock);
    struct timespec ts;
    ret = clock_gettime(CLOCK_REALTIME, &ts);    //CLOCK_REALTIME, CLOCK_MONOTONIC
    if(ret != 0)
    {
        printf("fatal error! clock gettime ret[%d]\n", ret);
    }
    long long curr0 = ((long long)(ts.tv_sec)*1000000000LL + ts.tv_nsec)/1000LL;
    printf("curr0:[%lld]ms\n", curr0/1000);
    int relative_sec = timeout/1000;
    int relative_nsec = (timeout%1000)*1000000;
    ts.tv_sec += relative_sec;
    ts.tv_nsec += relative_nsec;
    ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
    ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
    long long wantTm = ((long long)(ts.tv_sec)*1000000000LL + ts.tv_nsec)/1000LL;
    printf("wantTm:[%lld]ms, itl:[%lld]ms\n", wantTm/1000, wantTm/1000-curr0/1000);
    ret = pthread_cond_timedwait(&testCondition, &testLock, &ts);
    if(ETIMEDOUT == ret)
    {
        printf("pthread cond timeout[%d]ms np ret[%d]\n", timeout, ret);
        ret = clock_gettime(CLOCK_REALTIME, &ts);    //CLOCK_REALTIME, CLOCK_MONOTONIC
        if(ret != 0)
        {
            printf("fatal error! clock gettime ret[%d]\n", ret);
        }
        long long curr1 = ((long long)(ts.tv_sec)*1000000000LL + ts.tv_nsec)/1000LL;
        printf("curr1:[%lld]ms\n", curr1/1000);
        printf("real time duration is [%lld]ms\n", (curr1-curr0)/1000);
    }
    else if(0 == ret)
    {
    }
    else
    {
        printf("fatal error! pthread cond timedwait[%d]\n", ret);
        ret = clock_gettime(CLOCK_REALTIME, &ts);    //CLOCK_REALTIME, CLOCK_MONOTONIC
        if(ret != 0)
        {
            printf("fatal error! clock gettime ret[%d]\n", ret);
        }
        long long curr1 = ((long long)(ts.tv_sec)*1000000000LL + ts.tv_nsec)/1000LL;
        printf("fatal error! real time duration is [%lld]ms\n", (curr1-curr0)/1000);
    }
    pthread_mutex_unlock(&testLock);

    pthread_cond_destroy(&testCondition);
	pthread_mutex_destroy(&testLock);
    return result;
}
FINSH_FUNCTION_EXPORT_ALIAS(test_hello, __cmd_test_hello, 'test hello, world');

