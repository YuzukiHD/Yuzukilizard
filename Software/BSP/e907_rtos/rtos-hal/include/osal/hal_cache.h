#ifndef SUNXI_HAL_CACHE_H
#define SUNXI_HAL_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_KERNEL_FREERTOS
#ifndef CONFIG_CORE_DSP0
#include <mmu_cache.h>
#endif
#elif defined(CONFIG_OS_MELIS)
#include <arch.h>
#else
#error "can not support unknown platform"
#endif

void hal_dcache_clean(unsigned long vaddr_start, unsigned long size);
void hal_dcache_invalidate(unsigned long vaddr_start, unsigned long size);
void hal_dcache_clean_invalidate(unsigned long vaddr_start, unsigned long size);
void hal_icache_invalidate_all(void);
void hal_icache_invalidate(unsigned long vaddr_start, unsigned long size);
void hal_dcache_invalidate_all(void);
void hal_dcache_clean_all(void);

void hal_dcache_init(void);
void hal_icache_init(void);

#ifdef __cplusplus
}
#endif

#endif
