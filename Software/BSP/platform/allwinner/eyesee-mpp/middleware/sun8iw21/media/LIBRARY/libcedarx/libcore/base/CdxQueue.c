/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxQueue.c
 * Description : Queue
 * History :
 *
 */

#include <CdxTypes.h>
#include <CdxAtomic.h>
#include <CdxMemory.h>
//#include <CdxBaseErrno.h>
#include <cdx_log.h>

#include <CdxQueue.h>

#define CdxAtomicBoolCAS(ptr, oldVal, newVal) \
            __sync_bool_compare_and_swap(ptr, oldVal, newVal)

struct CdxQueueImplS
{
    struct CdxQueueS base;
    struct CdxQueueNodeEntityS *front;
    struct CdxQueueNodeEntityS *rear;
    cdx_bool enablePop;
    cdx_bool enablePush;
    AwPoolT *pool;
};

struct CdxQueueNodeEntityS
{
    struct CdxQueueNodeEntityS *next;
    cdx_atomic_t ref;
    CdxQueueDataT data;
};

static inline struct CdxQueueNodeEntityS *
        QueueNodeEntityIncRef(struct CdxQueueNodeEntityS *entity)
{
    CdxAtomicInc(&entity->ref);
    return entity;
}

static inline void QueueNodeEntityDecRef(AwPoolT *pool, struct CdxQueueNodeEntityS *entity)
{
    if (CdxAtomicDec(&entity->ref) == 0)
    {
        Pfree(pool, entity);
    }
}

static CdxQueueDataT __CdxQueuePop(CdxQueueT *queue)
{
    struct CdxQueueImplS *impl;
    struct CdxQueueNodeEntityS *entity = NULL;
    CdxQueueDataT data;
    CDX_CHECK(queue);
    impl = CdxContainerOf(queue, struct CdxQueueImplS, base);

    if (!impl->enablePop)
    {
        return NULL;
    }

    do
    {
        if (entity)
        {
            QueueNodeEntityDecRef(impl->pool, entity);
        }
        entity = QueueNodeEntityIncRef(impl->front);
        if (entity->next == NULL) /* it's only a dummy node */
        {
            QueueNodeEntityDecRef(impl->pool, entity);
            return NULL;
        }
        data = entity->next->data;
        /*
              *先把数据保存下来，
              *避免取到entity之后，它的next被释放了
              */
    }
    while (!CdxAtomicBoolCAS(&impl->front, entity, entity->next));
    QueueNodeEntityDecRef(impl->pool, entity); /*对应上面取entity的时候+1*/
    QueueNodeEntityDecRef(impl->pool, entity); /*再-1 才能释放内存*/

    return data;
}

static cdx_err __CdxQueuePush(CdxQueueT *queue, CdxQueueDataT data)
{
    struct CdxQueueImplS *impl;
    struct CdxQueueNodeEntityS *entity, *tmpEntity;
    cdx_bool ret;

    CDX_CHECK(queue);
    impl = CdxContainerOf(queue, struct CdxQueueImplS, base);

    if (!impl->enablePush)
    {
        return -1;
    }
    entity = Palloc(impl->pool, sizeof(*entity));
    CDX_FORCE_CHECK(entity);
    CDX_CHECK(data);/*不希望有为0的data*/
    entity->data = data;
    entity->next = NULL;
    CdxAtomicSet(&entity->ref, 1);

    do
    {
        tmpEntity = impl->rear;
    }
    while (!CdxAtomicBoolCAS(&tmpEntity->next, NULL, entity));

    ret = CdxAtomicBoolCAS(&impl->rear, tmpEntity, entity);
    CDX_CHECK(CDX_TRUE == ret);

    return CDX_SUCCESS;
}

static cdx_bool __CdxQueueEmpty(CdxQueueT *queue)
{
    struct CdxQueueImplS *impl;

    CDX_CHECK(queue);
    impl = CdxContainerOf(queue, struct CdxQueueImplS, base);

    return impl->front == impl->rear;
}

static struct CdxQueueOpsS gQueueOps =
{
    .pop = __CdxQueuePop,
    .push = __CdxQueuePush,
    .empty = __CdxQueueEmpty
};

CdxQueueT *CdxQueueCreate(AwPoolT *pool)
{
    struct CdxQueueImplS *impl;
    struct CdxQueueNodeEntityS *dummy;
    impl = Palloc(pool, sizeof(struct CdxQueueImplS));
    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(struct CdxQueueImplS));

    impl->pool = pool;
    dummy = Palloc(impl->pool, sizeof(struct CdxQueueNodeEntityS));
    CDX_FORCE_CHECK(dummy);
    dummy->next = NULL;
    dummy->data = NULL;
    CdxAtomicSet(&dummy->ref, 1);

    impl->front = dummy;
    impl->rear = dummy;
    impl->base.ops = &gQueueOps;
    impl->enablePop = CDX_TRUE;
    impl->enablePush = CDX_TRUE;
    return &impl->base;
}

void CdxQueueDestroy(CdxQueueT *queue)
{
    struct CdxQueueImplS *impl;
//    struct CdxQueueNodeEntityS *dummy;

    CDX_CHECK(queue);
    impl = CdxContainerOf(queue, struct CdxQueueImplS, base);

    impl->enablePush = CDX_FALSE;

    impl->enablePop = CDX_FALSE;
    CDX_LOG_CHECK(impl->front == impl->rear, "queue not empty");

    QueueNodeEntityDecRef(impl->pool, impl->front);
    Pfree(impl->pool, impl);

    return ;
}

