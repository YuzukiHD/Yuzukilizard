//#define LOG_NDEBUG 0
#define LOG_TAG "sample_pthread_cancel"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "sample_pthread_cancel.h"

static void* SampleDetectInputThread(void* pThreadData)
{
    int ret;
    int nOldCancelState = 100;
    ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &nOldCancelState);
    if(ret != 0)
    {
        aloge("fatal error! pthread setcancelstate fail[%d]", ret);
    }
    alogd("set cancel state:%d, old:%d", PTHREAD_CANCEL_ENABLE, nOldCancelState);

    int nOldCancelType = 101;
    ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &nOldCancelType);
    if(ret != 0)
    {
        aloge("fatal error! pthread setcancelstate fail[%d]", ret);
    }
    alogd("set cancel type:%d, old:%d", PTHREAD_CANCEL_DEFERRED, nOldCancelType);
    
    SamplePthreadCancelContext* pContext = (SamplePthreadCancelContext*) pThreadData;

    alogd("detect input thread is running!");
    prctl(PR_SET_NAME, (unsigned long)"sample_pthread_cancel_DetectInput", 0, 0, 0);
    while(1)
    {
        alogd("before getchar.");
        char ch = getchar();
        alogd("after getchar[%c][0x%x].", ch, ch);
        if ( ch == 'q' || ch == '\03')
        {
            alogd("exit detect thread is exit. ch[%d]", ch);
            return (void*)1;
        }
        //alogd("before testcancel.");
        pthread_testcancel();
        //alogd("after testcancel.");
    }
    alogd("fatal error! exit detect thread is exit2.");
    return (void*)0;
}

SamplePthreadCancelContext* constructSamplePthreadCancelContext()
{
    SamplePthreadCancelContext *pContext = malloc(sizeof(SamplePthreadCancelContext));
    memset(pContext, 0, sizeof(SamplePthreadCancelContext));
    return pContext;
}
void destructSamplePthreadCancelContext(SamplePthreadCancelContext *pContext)
{
    free(pContext);
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    int ret;
    void *pValue = NULL;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 0,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);

    SamplePthreadCancelContext* pContext = constructSamplePthreadCancelContext();
    ret = pthread_create(&pContext->mDetectInputThreadId, NULL, SampleDetectInputThread, pContext);
    if(0 == ret)
    {
        alogd("pthread create threadId[0x%x]", pContext->mDetectInputThreadId);
    }
    else
    {
        aloge("fatal error! pthread create fail[%d]", ret);
    }
    sleep(5);
    ret = pthread_cancel(pContext->mDetectInputThreadId);
    alogd("post cancel signal to pthread[0x%lx] join, ret[0x%x]", pContext->mDetectInputThreadId, ret);
    ret = pthread_join(pContext->mDetectInputThreadId, &pValue);
    alogd("pthread[0x%lx] join, ret[0x%x], pValue[%p]", pContext->mDetectInputThreadId, ret, pValue);

    destructSamplePthreadCancelContext(pContext);
    pContext = NULL;
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

