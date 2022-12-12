/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : SmartDnsService.c
 * Description : SmartDnsService
 * History :
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include <CdxTypes.h>
#include <CdxList.h>
#include <CdxLock.h>
#include <CdxMessage.h>

#include <SmartDnsService.h>

#define TOTAL_LIFE_TIME_SEC (60 * 30) /* 30min */
#define POLL_TIMEOUT_US 1000000

enum SDSMsgTypeE
{
    SDS_MSG_NET_SEARCH,
    SDS_MSG_POLL,
};

struct DomainItemS
{
    char hostname[512];
    int port;
    struct addrinfo *addr;
    CdxListNodeT node;
    int lifetimeSec;
};

typedef struct SearchItemS
{
    struct SDSInstanceS *instance;
    char hostname[512];
    int port;
    void *userHdr;
    ResponeHook hook;
}SearchItemT;

struct SDSInstanceS
{
    CdxHandlerItfT msgHdrItf;
    CdxHandlerT *msgHdr;

    AwPoolT *pool;

    CdxListT DAList;
    CdxMutexT listMutex;

    CdxCondT cond;
    CdxMutexT condMutex;

};

static struct addrinfo *SDSCacheSearch(struct SDSInstanceS *instance,
    const char *hostname, int port)
{
    struct DomainItemS *pos = NULL;
    struct addrinfo *ai = NULL;
    CdxMutexLock(&instance->listMutex);
    CdxListForEachEntry(pos, (&instance->DAList), node)
    {
        if ((strcmp(hostname, pos->hostname) == 0) && (pos->port == port))
        {
            ai = pos->addr;
            break;
        }
    }
    CdxMutexUnlock(&instance->listMutex);

    return ai;
}

static struct addrinfo *SDSNetSearch(const char *hostname, int port)
{
    struct addrinfo *retAddr = NULL;
    int ret;
    cdx_char strPort[10] = {0};

    sprintf(strPort, "%d", port);

    ret = getaddrinfo(hostname, strPort, NULL, &retAddr);
    if (ret != 0)
    {
        CDX_LOGE("get host failed, host:%s, port:%s, err:%s", hostname,
            strPort, gai_strerror(errno));
        return NULL;
    }

    return retAddr;
}

static void *SDSNetSearchWrap(void* arg)
{
    SearchItemT *searchArg = (struct SearchItemS *)arg;
    struct SDSInstanceS *instance;
    struct addrinfo *ai,*searchAi;
    const char *hostname;
    int port;
    void *userHdr;
    ResponeHook hook;
    struct DomainItemS *item;

    pthread_detach(pthread_self());

    if (searchArg == NULL){
        pthread_exit(0);
        return NULL;
    }

    instance = searchArg->instance;
    hostname = searchArg->hostname;
    port = searchArg->port;
    userHdr = searchArg->userHdr;
    hook = searchArg->hook;
    ai = SDSNetSearch(hostname, port);

    if (ai == NULL)
    {
        hook(userHdr, SDS_ERROR, NULL);
        free(searchArg);
        searchArg = NULL;
        pthread_exit(0);
        return NULL;
    }

    //check if already have serach by other thread while SDSNetSearch()!
    searchAi = SDSCacheSearch(instance, hostname, port);
    if (searchAi)
    {
        logd("already have %s dns",hostname);
        freeaddrinfo(ai);
        hook(userHdr, SDS_OK, searchAi);
    }
    else
    {
        item = Palloc(instance->pool, sizeof(*item));
        memset(item, 0x00, sizeof(*item));
        strncpy(item->hostname, hostname, 511);
        item->addr = ai;
        item->port = port;
        item->lifetimeSec = TOTAL_LIFE_TIME_SEC;

        CdxMutexLock(&instance->listMutex);
        CdxListAdd(&item->node, &instance->DAList);
        CdxMutexUnlock(&instance->listMutex);
        hook(userHdr, SDS_OK, ai);
    }

    free(searchArg);
    searchArg = NULL;
    pthread_exit(0);

    return NULL;
}

static inline void SDSPollWrap(struct SDSInstanceS *instance)
{
    struct DomainItemS *item = NULL, *npos = NULL;
    CdxMutexLock(&instance->listMutex);
    CdxListForEachEntrySafe(item, npos, (&instance->DAList), node)
    {
        if (--(item->lifetimeSec) == 0)
        {
            freeaddrinfo(item->addr);
            CdxListDel(&item->node);
            Pfree(instance->pool, item);
        }
    }
    CdxMutexUnlock(&instance->listMutex);
    return;
}

static void __SDSMsgRecv(CdxHandlerItfT *itf, CdxMessageT *msg)
{
    struct SDSInstanceS *instance = NULL;
    instance = CdxContainerOf(itf, struct SDSInstanceS, msgHdrItf);

    switch (CdxMessageWhat(msg))
    {

    case SDS_MSG_POLL:
    {
        CdxMessageT *msg = NULL;
        struct timespec abstime;

        SDSPollWrap(instance);

        abstime.tv_sec = time(0) + 1;
        abstime.tv_nsec = 0;
        CdxMutexLock(&instance->condMutex);
        CdxCondTimedwait(&instance->cond, &instance->condMutex, &abstime); /* wait 1 second */
        CdxMutexUnlock(&instance->condMutex);

        msg = CdxMessageCreate(instance->pool, SDS_MSG_POLL, instance->msgHdr);
        CdxMessagePost(msg);
        break;
    }
    case SDS_MSG_NET_SEARCH:
    {
        //need to do search in thread not to block here!
        const char * hostname;
        SearchItemT *searchArg;
        searchArg = (SearchItemT *)calloc(1,sizeof(SearchItemT));
        searchArg->instance = instance;
        CdxMessageFindObject(msg, "hostname", &hostname);
        CdxMessageFindInt32(msg, "port", &searchArg->port);
        CdxMessageFindObject(msg, "userHdr", &searchArg->userHdr);
        CdxMessageFindObject(msg, "ResponeHook", &searchArg->hook);
        sprintf(searchArg->hostname,"%s",hostname);
        pthread_t pid;
        int ret = pthread_create(&pid, NULL, SDSNetSearchWrap, (void*)searchArg);
        if (ret != 0)
        {
            logd("dns search thread creat fail!");
            ResponeHook hook = searchArg->hook;
            hook(searchArg->userHdr, SDS_ERROR, NULL);
            free(searchArg);
            searchArg = NULL;
        }
        CdxMessageClearMeta(msg);
        break;
    }
    default:
        CDX_LOG_CHECK(0, "should not be here.");
        break;
    };

}

static struct CdxHandlerItfOpsS SDSHdrOps =
{
    .msgRecv = __SDSMsgRecv
};

static struct SDSInstanceS *SDSCreateInstance(void)
{
    struct SDSInstanceS *instance;
    instance = malloc(sizeof(*instance));

    instance->pool = AwPoolCreate(NULL);
    instance->msgHdrItf.ops = &SDSHdrOps;
    instance->msgHdr = CdxHandlerCreate(instance->pool, &instance->msgHdrItf, NULL);

    CdxListInit(&instance->DAList);

    CdxMutexInit(&instance->listMutex);

    CdxMutexInit(&instance->condMutex);
    CdxCondInit(&instance->cond);

    return instance;
}

static struct SDSInstanceS *SDSGetInstance(void)
{
    static struct SDSInstanceS *singletonInstance = NULL;

    if (singletonInstance == NULL)
    {
        singletonInstance = SDSCreateInstance();
    }

    return singletonInstance;
}

int SDSRequest(const char *hostname, int port, struct addrinfo **pAddr, void *userHdr,
    ResponeHook hook)
{
    CdxMessageT *msg = NULL;
    struct SDSInstanceS *instance = NULL;
    struct addrinfo *ai = NULL;

    if (hostname == NULL)
    {
        CDX_LOGE("hostname null...");
        return -1;
    }

    instance = SDSGetInstance();
    ai = SDSCacheSearch(instance, hostname, port);
    if (ai)
    {
        *pAddr = ai;
        return SDS_OK;
    }
    msg = CdxMessageCreate(instance->pool, SDS_MSG_NET_SEARCH, instance->msgHdr);
    CdxMessageSetObject(msg, "hostname", (void *)hostname);
    CdxMessageSetInt32(msg, "port", port);
    CdxMessageSetObject(msg, "userHdr", userHdr);
    CdxMessageSetObject(msg, "ResponeHook", hook);
    CdxMessagePost(msg);

    CdxMutexLock(&instance->condMutex);
    CdxCondSignal(&instance->cond); /* wait 1 second */
    CdxMutexUnlock(&instance->condMutex);

    return SDS_PENDING;
}

