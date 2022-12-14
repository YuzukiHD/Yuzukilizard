#include <kapi.h>
#include <string.h>
#include <stdlib.h>
#include "cache_lock.h"
#include <log.h>

static void impl_cachelock_unlock(cache_lock_t *thiz)
{
    pthread_mutex_unlock(&thiz->reallock);
}

static void impl_cachelock_lock(cache_lock_t *thiz)
{
    pthread_mutex_lock(&thiz->reallock);
}

static __s32 impl_cachelock_trylock(cache_lock_t *thiz)
{
    __s32 ret = pthread_mutex_trylock(&thiz->reallock);
    return !ret ? EPDK_FAIL : EPDK_OK;
}

__s32 cache_lock_init(cache_lock_t *thiz)
{
    memset(thiz, 0, sizeof(cache_lock_t));
    if (pthread_mutex_init(&thiz->reallock, NULL))
    {
        __err("cachelock init fail, create locker fail.\n");
    }

    thiz->unlock = impl_cachelock_unlock;
    thiz->lock = impl_cachelock_lock;
    thiz->trylock = impl_cachelock_trylock;

    return EPDK_OK;
}

__s32 cache_lock_destroy(cache_lock_t *thiz)
{
    pthread_mutex_destroy(&thiz->reallock);

    return EPDK_OK;
}

cache_lock_t *new_cache_lock(void)
{
    __s32   ret;
    cache_lock_t *pcache_lock;

    pcache_lock = (cache_lock_t *)malloc(sizeof(cache_lock_t));
    if (NULL == pcache_lock)
    {
        __wrn("pcache_lock_t = NULL, malloc fail\n");
        return NULL;
    }

    ret = cache_lock_init(pcache_lock);
    if (EPDK_OK != ret)
    {
        __wrn("cache_lock_t Init fail, check code!\n");
        goto __err0;
    }

    return (cache_lock_t *)pcache_lock;

__err0:
    free(pcache_lock);
    return NULL;
}

void delete_cache_lock(cache_lock_t *thiz)
{
    __s32 ret;

    ret = cache_lock_destroy(thiz);
    if (EPDK_OK != ret)
    {
        __wrn("cache_lock_t exit fail\n");
    }

    free(thiz);
}
