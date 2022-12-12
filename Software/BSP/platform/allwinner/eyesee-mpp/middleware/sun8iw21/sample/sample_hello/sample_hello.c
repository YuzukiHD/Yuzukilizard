/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_face_detect.c
  Version       : V6.0
  Author        : Allwinner BU3-XIAN Team
  Created       :
  Last Modified : 2017/11/30
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

//#include <string.h>
//#include <pthread.h>
//#include <assert.h>
//#include <time.h>

#include <plat_log.h>
#include <mpi_sys.h>
#include <ion_memmanager.h>
#include "sample_hello.h"

/*int main(int argc, char *argv[])
{
    printf("hello, world! [%s][%s][%s][%d][%s].\n", __DATE__, __TIME__, __FILE__, __LINE__, __FUNCTION__);
    int ret = 0;

    wordexp_t p;
    char **w;
    int i;

    wordexp("ls -al *.c", &p, 0);
    w = p.we_wordv;
    for (i = 0; i < p.we_wordc; i++)
        printf("%s\n", w[i]);
    wordfree(&p);

    return ret;
}*/

typedef struct 
{
    volatile int counter;
} SampleHelloAtomic;

int SampleHelloAtomicSet(SampleHelloAtomic *ref, int val)
{
    return __sync_lock_test_and_set(&ref->counter, val);
}

int main(int argc, char *argv[])
{
    int result = 0;
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
    strcpy(stGLogConfig.LogFileNameExtension, "");
    log_init(argv[0], &stGLogConfig);

    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    AW_MPI_SYS_Init();
    unsigned int nPhyAddr = 0;
    void* pVirtAddr = NULL;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, 128*1024);
    if(ret != SUCCESS)
    {
        aloge("fatal error! malloc fail!");
    }
    else
    {
        alogd("phyAddr[0x%x], virtAddr[%p]", nPhyAddr, pVirtAddr);
    }
    int i=0;
    for(i=0; i<30; i++)
    {
        memset(pVirtAddr, i, 64*1024);
        alogd("memset [%d]", i);
        alogd("result: pVirtAddr:0x%x,%x,%x", ((char*)pVirtAddr)[0], ((char*)pVirtAddr)[1], ((char*)pVirtAddr)[2]);
        sleep(1);
    }
    AW_MPI_SYS_MmzFree(0, pVirtAddr);


    SampleHelloAtomic stAtomic;
    stAtomic.counter = 15;
    alogd("before atomic set:[%d]", stAtomic.counter);
    SampleHelloAtomicSet(&stAtomic, 3);
    alogd("after atomic set:[%d]", stAtomic.counter);

    AW_MPI_SYS_Exit();
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

