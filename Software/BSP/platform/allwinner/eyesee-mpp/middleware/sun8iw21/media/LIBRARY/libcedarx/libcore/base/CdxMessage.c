/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMessage.c
 * Description : Message
 * History :
 *
 */

#include <CdxMessage.h>

#include <CdxMemory.h>
#include <CdxLock.h>
//#include <CdxBaseErrno.h>

#include <CdxAtomic.h>
#include <CdxList.h>
#include <CdxQueue.h>
#include <unistd.h>

struct CdxEventS
{
    struct CdxMessageS *msg;
    struct CdxDeliverS *deliver;
    CdxListNodeT deliverListNode;
    CdxListNodeT timerListNode;
    cdx_uint32 timeout; /*100 millisecond*/
};

struct CdxDeliverTimerCtxS
{
    CdxListT timeoutList;
    CdxMutexT timeoutListMutex;
    pthread_t timeoutProcPid;

    cdx_bool threadExit;
};

struct CdxDeliverTimerCtxS *gDeliverTimerHdr;

/*用队列存储消息，读写之间免锁*/
static void *DeliverTimerProcess(void *arg)
{
    struct CdxDeliverTimerCtxS *ctx = (struct CdxDeliverTimerCtxS *)arg;
    struct CdxEventS *posEvent, *nextEvent;

    while(!ctx->threadExit)
    {
        usleep(100000);
        CdxMutexLock(&ctx->timeoutListMutex); /*lock*/
        CdxListForEachEntrySafe(posEvent, nextEvent, &ctx->timeoutList, timerListNode)
        {
            if (--posEvent->timeout == 0)
            {
                CdxListDel(&posEvent->deliverListNode);
                CdxListDel(&posEvent->timerListNode);
                CdxDeliverPostUs(posEvent->deliver, posEvent->msg, 0);
                Pfree(NULL, posEvent);
            }
        }
        CdxMutexUnlock(&ctx->timeoutListMutex); /*lock*/
    }
    return NULL;
}

//static void DeliverTimerInit(void) __attribute__((constructor));
void DeliverTimerInit(void)
{
    cdx_int32 ret;
    gDeliverTimerHdr = Palloc(NULL, sizeof(*gDeliverTimerHdr));
    CDX_FORCE_CHECK(gDeliverTimerHdr);

    memset(gDeliverTimerHdr, 0x00, sizeof(*gDeliverTimerHdr));

    CdxMutexInit(&gDeliverTimerHdr->timeoutListMutex);
    CdxListInit(&gDeliverTimerHdr->timeoutList);

    gDeliverTimerHdr->threadExit = CDX_FALSE;

    ret = pthread_create(&gDeliverTimerHdr->timeoutProcPid, NULL,
                        DeliverTimerProcess, gDeliverTimerHdr);
    CDX_FORCE_CHECK(ret == 0);

    return ;
}

//static void DeliverTimerExit(void) __attribute__((destructor));
void DeliverTimerExit(void)
{
    struct CdxEventS *posEvent, *nextEvent;
    gDeliverTimerHdr->threadExit = CDX_TRUE;
    pthread_join(gDeliverTimerHdr->timeoutProcPid, NULL);

    CdxListForEachEntrySafe(posEvent, nextEvent, &gDeliverTimerHdr->timeoutList, timerListNode)
    {
        CdxListDel(&posEvent->timerListNode);
        CdxMessageDestroy(posEvent->msg);
        Pfree(NULL, posEvent);
    }

    CdxMutexDestroy(&gDeliverTimerHdr->timeoutListMutex);
    Pfree(NULL, gDeliverTimerHdr);
    gDeliverTimerHdr = NULL;
    return ;
}

struct CdxDeliverImplS
{
    CdxDeliverT base;
    CdxQueueT *eventQueue;

    CdxListT timeoutList;

    pthread_t pid;
    CdxMutexT mutex;
    CdxCondT cond;
    volatile cdx_bool threadExit;
    AwPoolT *pool; /*会起一个独立线程，用独立的pool*/
};

CdxDeliverT *globalDeliver = NULL;

static cdx_int32 __CdxDeliverPostUS(CdxDeliverT *deliver, CdxMessageT *msg,
                        cdx_uint64 timeout)
{
    struct CdxDeliverImplS *impl;

    impl = CdxContainerOf(deliver, struct CdxDeliverImplS, base);

    if (timeout == 0LL)
    {
        CdxQueuePush(impl->eventQueue, msg);
        CdxMutexLock(&impl->mutex); /*lock*/
        CdxCondSignal(&impl->cond);
        CdxMutexUnlock(&impl->mutex); /*unlock*/
    }
    else
    {
        struct CdxEventS *event;

        if (!gDeliverTimerHdr)
        {
            DeliverTimerInit();
        }

        event = Palloc(0, sizeof(*event));
        CDX_FORCE_CHECK(event);

        event->msg = msg;
        event->timeout = timeout/100000;
        event->deliver = deliver;

        CdxListAdd(&event->deliverListNode, &impl->timeoutList);

        CdxMutexLock(&gDeliverTimerHdr->timeoutListMutex); /*lock*/
        CdxListAddTail(&event->timerListNode, &gDeliverTimerHdr->timeoutList);
        CdxMutexUnlock(&gDeliverTimerHdr->timeoutListMutex); /*lock*/
    }
    return CDX_SUCCESS;
}

struct CdxDeliverOpsS deliverOps =
{
    .postUS = __CdxDeliverPostUS
};

/*用队列存储消息，读写之间就免锁了*/
static void *DeliverProcess(void *arg)
{
    struct CdxDeliverImplS *impl = (struct CdxDeliverImplS *)arg;
    CdxMessageT *msg;
    // CDX_LOGD("DeliverProcess (%d)", gettid());

    for (;;)
    {

        if (CdxQueueEmpty(impl->eventQueue))
        {
            if (impl->threadExit)
            {
                break;
            }
            struct timespec abstime;
            abstime.tv_sec = time(0) + 1;
            abstime.tv_nsec = 0;
            CdxMutexLock(&impl->mutex); /*lock*/
            CdxCondTimedwait(&impl->cond, &impl->mutex, &abstime);
            CdxMutexUnlock(&impl->mutex); /*unlock*/
        }

        if (impl->threadExit)
        {
            break;
        }

        while (!CdxQueueEmpty(impl->eventQueue))
        {
            msg = CdxQueuePop(impl->eventQueue);
            CDX_CHECK(msg);
            if (msg)
            {
                CdxMessageDeliver(msg);
                CdxMessageDestroy(msg);
            }
        }

        if (impl->threadExit)
        {
            break;
        }

    }

    pthread_exit(NULL);
    return NULL;
}

CdxDeliverT *CdxDeliverCreate(AwPoolT *pool)
{
    cdx_int32 ret;
    AwPoolT *selfPool = NULL;
    struct CdxDeliverImplS *impl;

    selfPool = AwPoolCreate(pool); /*create self pool will fast deliver*/
    impl = Palloc(selfPool, sizeof(*impl));
    CDX_FORCE_CHECK(impl);

    memset(impl, 0x00, sizeof(*impl));
    impl->pool = selfPool;
    impl->base.ops = &deliverOps;

    CdxMutexInit(&impl->mutex);
    CdxCondInit(&impl->cond);
    CdxListInit(&impl->timeoutList);

    impl->eventQueue = CdxQueueCreate(impl->pool);

    impl->threadExit = CDX_FALSE;

    ret = pthread_create(&impl->pid, NULL, DeliverProcess, impl);
    CDX_LOGD("deliver process, pid(%ld)", (unsigned long)impl->pid);
    CDX_FORCE_CHECK(ret == 0);

    return &impl->base;
}

void CdxDeliverClearMsg(CdxDeliverT *deliver)
{
    struct CdxDeliverImplS *impl;
    struct CdxEventS *event, *nEvent;
    CdxMessageT *msg;
    impl = CdxContainerOf(deliver, struct CdxDeliverImplS, base);

    CdxListForEachEntrySafe(event, nEvent, &impl->timeoutList, deliverListNode)
    {
        CdxListDel(&event->timerListNode);
        CdxListDel(&event->deliverListNode);
        CdxMessageDestroy(event->msg);
        Pfree(0, event);
    }

    while (!CdxQueueEmpty(impl->eventQueue))
    {
        msg = CdxQueuePop(impl->eventQueue);
        CdxMessageDestroy(msg);
    }
    return ;
}

void CdxDeliverDestroy(CdxDeliverT *deliver)
{
    struct CdxDeliverImplS *impl;
    AwPoolT *selfPool;

    impl = CdxContainerOf(deliver, struct CdxDeliverImplS, base);

    CdxDeliverClearMsg(deliver);
    selfPool = impl->pool;
    impl->threadExit = CDX_TRUE;

    CdxMutexLock(&impl->mutex); /*lock*/
    CdxCondBroadcast(&impl->cond);
    CdxMutexUnlock(&impl->mutex); /*unlock*/

    CDX_LOGD("wait thread, pid(%ld)", (unsigned long)impl->pid);
    pthread_join(impl->pid, NULL);

    CdxDeliverClearMsg(deliver);
    CDX_CHECK(CdxQueueEmpty(impl->eventQueue));
    CdxDeliverClearMsg(deliver);
    CdxQueueDestroy(impl->eventQueue);
    CdxMutexDestroy(&impl->mutex);
    CdxCondDestroy(&impl->cond);
    Pfree(impl->pool, impl);
    AwPoolDestroy(selfPool);

    return ;
}

void CdxDeliverReset(void)
{
    if (globalDeliver)
    {
        CdxDeliverDestroy(globalDeliver);
        globalDeliver = NULL;
    }
    else
    {
        CDX_LOGW("global deliver not initinal...");
    }
    DeliverTimerExit();
}

/**************************message impl*********************************/
struct CdxMessageImplS
{
    struct CdxMessageS base;
    CdxMetaT *meta;
    AwPoolT *pool;
    CdxHandlerT *handler;
    cdx_int32 what;
    cdx_atomic_t ref;
};

static struct CdxMessageS *__CdxMessageIncRef(struct CdxMessageS *msg)
{
    struct CdxMessageImplS *impl;
    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    CdxAtomicInc(&impl->ref);
    return msg;
}

static void __CdxMessageDecRef(struct CdxMessageS *msg)
{
    struct CdxMessageImplS *impl;

    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    if (CdxAtomicDec(&impl->ref) == 0)
    {
        CdxMetaDestroy(impl->meta);
        Pfree(impl->pool, impl);
    }
    return ;
}

static void __CdxMessageDeliver(CdxMessageT *msg)
{
    struct CdxMessageImplS *impl;

    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    CdxHandlerMsgRecv(impl->handler, msg);
    return ;
}

static CdxMetaT *__CdxMessageGetMeta(CdxMessageT *msg)
{
    struct CdxMessageImplS *impl;
    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    return impl->meta;
}

static cdx_int32 __CdxMessageWhat(CdxMessageT *msg)
{
    struct CdxMessageImplS *impl;
    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    return impl->what;
}

static cdx_err __CdxMessagePostUs(struct CdxMessageS *msg, cdx_uint64 timeout)
{
    struct CdxMessageImplS *impl;
    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    return CdxHandlerPostUs(impl->handler, msg, timeout);
}

static CdxMessageT *__CdxMessageDup(CdxMessageT *msg)
{
    struct CdxMessageImplS *impl, *newImpl;
    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);

    newImpl = Palloc(impl->pool, sizeof(struct CdxMessageImplS));
    CDX_FORCE_CHECK(newImpl);
    memset(newImpl, 0x00, sizeof(struct CdxMessageImplS));

    newImpl->meta = CdxMetaDup(impl->meta);
    newImpl->what = impl->what;
    newImpl->handler = impl->handler;
    CdxAtomicSet(&newImpl->ref, 1);
    newImpl->base.ops = impl->base.ops;

    return &newImpl->base;
}

struct CdxMessageOpsS messageOps =
{
    .incRef = __CdxMessageIncRef,
    .decRef = __CdxMessageDecRef,

    .getMeta = __CdxMessageGetMeta,
    .what = __CdxMessageWhat,
    .postUs = __CdxMessagePostUs,
    .dup = __CdxMessageDup,
    .deliver = __CdxMessageDeliver
};

CdxMessageT *__CdxMessageCreate(AwPoolT *pool, cdx_int32 waht, CdxHandlerT *handler,
                    char *file, int line)
{
    struct CdxMessageImplS *impl;
    char *callFrom = file;

#ifdef MEMORY_LEAK_CHK
    callFrom = malloc(512);
    sprintf(callFrom, "%s:%s", file, __FUNCTION__);
#endif

    impl = AwPalloc(pool, sizeof(struct CdxMessageImplS), callFrom, line);
    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(struct CdxMessageImplS));
    CDX_CHECK(handler);
    impl->pool = pool;
    impl->meta = CdxMetaCreate(impl->pool);
    impl->what = waht;
    impl->handler = handler;
    CdxAtomicSet(&impl->ref, 1);
    impl->base.ops = &messageOps;

    return &impl->base;
}

void CdxMessageDestroy(CdxMessageT  *msg)
{
    struct CdxMessageImplS *impl;
    CDX_CHECK(msg);
    impl = CdxContainerOf(msg, struct CdxMessageImplS, base);
    CDX_CHECK(CdxAtomicRead(&impl->ref) == 1);
    __CdxMessageDecRef(msg);
    return ;
}

/*-----------------------------msg handler-----------------------------*/
struct CdxHandlerImplS
{
    struct CdxHandlerS base;
    CdxDeliverT *deliver;
    CdxHandlerItfT *itf;
    AwPoolT *pool;
};

cdx_int32 __CdxHandlerPostUS(CdxHandlerT *hdr, CdxMessageT *msg, cdx_uint64 timeUs)
{
    struct CdxHandlerImplS *impl;
    impl = CdxContainerOf(hdr, struct CdxHandlerImplS, base);

    return CdxDeliverPostUs(impl->deliver, msg, timeUs);
}

void __CdxHandlerMsgRecv(CdxHandlerT *hdr, CdxMessageT *msg)
{
    struct CdxHandlerImplS *impl;
    impl = CdxContainerOf(hdr, struct CdxHandlerImplS, base);

    impl->itf->ops->msgRecv(impl->itf, msg);
    return ;
}

struct CdxHandlerOpsS handlerOps =
{
    .postUs = __CdxHandlerPostUS,
    .msgRecv = __CdxHandlerMsgRecv
};

CdxHandlerT *CdxHandlerCreate(AwPoolT *pool, CdxHandlerItfT *itf, CdxDeliverT *deliver)
{
    struct CdxHandlerImplS *impl;

    impl = Palloc(pool, sizeof(*impl));
    impl->pool = pool;
    impl->base.ops = &handlerOps;
    impl->itf = itf;
    if (deliver)
    {
        impl->deliver = deliver;
    }
    else
    {
        if (!globalDeliver)
        {
            globalDeliver = CdxDeliverCreate(NULL);
        }
        impl->deliver = globalDeliver;
    }
    return &impl->base;
}

void CdxHandlerDestroy(CdxHandlerT *hdr)
{
    struct CdxHandlerImplS *impl;
    impl = CdxContainerOf(hdr, struct CdxHandlerImplS, base);

    Pfree(impl->pool, impl);
    return ;
}

/*--------------------------msg handler end----------------------------*/

