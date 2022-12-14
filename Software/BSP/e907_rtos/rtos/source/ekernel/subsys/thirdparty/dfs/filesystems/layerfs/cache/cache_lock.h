#ifndef CACHE_LOCK_H
#define CACHE_LOCK_H

#include <pthread.h>

typedef struct tag_cache_lock_t cache_lock_t;
typedef void (*cachelock_unlock)(cache_lock_t *thiz);
typedef void (*cachelock_lock)(cache_lock_t *thiz);
typedef __s32(*cachelock_trylock)(cache_lock_t *thiz);

typedef struct tag_cache_lock_t
{
    pthread_mutex_t     reallock;
    cachelock_unlock    unlock;
    cachelock_lock      lock;
    /* trt lock */
    cachelock_trylock   trylock;
} cache_lock_t;

extern __s32 cache_lock_init(cache_lock_t *thiz);
extern __s32 cache_lock_destroy(cache_lock_t *thiz);
extern cache_lock_t *new_cache_lock(void);
extern void delete_cache_lock(cache_lock_t *thiz);


#endif  /*CACHE_LOCK_H*/
