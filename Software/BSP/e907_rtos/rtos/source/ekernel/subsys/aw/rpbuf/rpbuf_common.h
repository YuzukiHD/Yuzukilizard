#ifndef __RPBUF_COMMON_H__
#define __RPBUF_COMMON_H__

#include <stdio.h>
#include <string.h>
#include <hal_log.h>
#include <hal_mutex.h>
#include <hal_atomic.h>
#include <hal_mem.h>
#include <hal_cache.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef printfFromISR
#define printfFromISR printf
#endif

#define RPBUF_LOG_COLOR_NONE		"\e[0m"
#define RPBUF_LOG_COLOR_RED		"\e[31m"
#define RPBUF_LOG_COLOR_GREEN		"\e[32m"
#define RPBUF_LOG_COLOR_YELLOW	"\e[33m"
#define RPBUF_LOG_COLOR_BLUE		"\e[34m"

#ifdef RPBUF_DBG
#define rpbuf_dbg(fmt, args...) \
	printf(RPBUF_LOG_COLOR_GREEN "[RPBUF_DBG][%s:%d]" fmt \
		RPBUF_LOG_COLOR_NONE, __func__, __LINE__, ##args)
#define rpbuf_dbgFromISR(fmt, args...) \
	printfFromISR(RPBUF_LOG_COLOR_GREEN "[RPBUF_DBG][%s:%d]" fmt \
		RPBUF_LOG_COLOR_NONE, __func__, __LINE__, ##args)
#else
#define rpbuf_dbg(fmt, args...)
#define rpbuf_dbgFromISR(fmt, args...)
#endif /* RPBUF_DBG */

#define rpbuf_info(fmt, args...) \
	printf(RPBUF_LOG_COLOR_BLUE "[RPBUF_INFO][%s:%d]" fmt \
		RPBUF_LOG_COLOR_NONE, __func__, __LINE__, ##args)
#define rpbuf_infoFromISR(fmt, args...) \
	printfFromISR(RPBUF_LOG_COLOR_BLUE "[RPBUF_INFO][%s:%d]" fmt \
		RPBUF_LOG_COLOR_NONE, __func__, __LINE__, ##args)

#define rpbuf_err(fmt, args...) \
	printf(RPBUF_LOG_COLOR_RED "[RPBUF_ERR][%s:%d]" fmt \
		RPBUF_LOG_COLOR_NONE, __func__, __LINE__, ##args)
#define rpbuf_errFromISR(fmt, args...) \
	printfFromISR(RPBUF_LOG_COLOR_RED "[RPBUF_ERR][%s:%d]" fmt \
		RPBUF_LOG_COLOR_NONE, __func__, __LINE__, ##args)

typedef hal_mutex_t rpbuf_mutex_t;
typedef hal_spinlock_t rpbuf_spinlock_t;

/* Mutex */
static inline rpbuf_mutex_t rpbuf_mutex_create(void)
{
	return hal_mutex_create();
}

static inline int rpbuf_mutex_delete(rpbuf_mutex_t mutex)
{
	return hal_mutex_delete(mutex);
}

static inline int rpbuf_mutex_lock(rpbuf_mutex_t mutex)
{
	return hal_mutex_lock(mutex);
}

static inline int rpbuf_mutex_unlock(rpbuf_mutex_t mutex)
{
	return hal_mutex_unlock(mutex);
}

/* Spin lock */
static inline void rpbuf_spin_lock(rpbuf_spinlock_t *lock)
{
	hal_spin_lock(lock);
}

static inline void rpbuf_spin_unlock(rpbuf_spinlock_t *lock)
{
	hal_spin_unlock(lock);
}

static inline uint32_t rpbuf_spin_lock_irqsave(rpbuf_spinlock_t *lock)
{
	return hal_spin_lock_irqsave(lock);
}

static inline void rpbuf_spin_unlock_irqrestore(rpbuf_spinlock_t *lock, uint32_t __cpsr)
{
	hal_spin_unlock_irqrestore(lock, __cpsr);
}

/* Dynamically allocate/free memory */
static inline void *rpbuf_malloc(unsigned int size)
{
	return hal_malloc(size);
}

static inline void *rpbuf_zalloc(unsigned int size)
{
	void *p = rpbuf_malloc(size);
	if (!p)
		return NULL;
	memset(p, 0, size);
	return p;
}

static inline void rpbuf_free(void *p)
{
	hal_free(p);
}

/* Cache */
static inline void rpbuf_dcache_clean(void *addr, unsigned long size)
{
	hal_dcache_clean((unsigned long)addr, size);
}

static inline void rpbuf_dcache_invalidate(void *addr, unsigned long size)
{
	hal_dcache_invalidate((unsigned long)addr, size);
}

#ifdef __cplusplus
}
#endif

#endif /* ifndef __RPBUF_COMMON_H__ */
