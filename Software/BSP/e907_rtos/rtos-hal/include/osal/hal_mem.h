#ifndef SUNXI_HAL_MEM_H
#define SUNXI_HAL_MEM_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

void *hal_realloc(void *ptr, uint32_t size);
void *hal_calloc(uint32_t numb, uint32_t size);

void *hal_malloc(uint32_t size);
void hal_free(void *p);

void *hal_malloc_align(uint32_t size, int align);
void hal_free_align(void *p);

void dma_alloc_coherent_init(void);
void dma_free_coherent_prot(void *addr);
void *dma_alloc_coherent_prot(size_t size, unsigned long prot);
void *dma_alloc_coherent_non_cacheable(size_t size);
void dma_free_coherent_non_cacheable(void *addr);

int dma_coherent_heap_init(void);
void *dma_coherent_heap_alloc(size_t size);
void dma_coherent_heap_free(void *ptr);

void *dma_coherent_heap_alloc_align(size_t size,int align);
void dma_coherent_heap_free_align(void *ptr);

unsigned long get_memory_prot(unsigned long prot);

#ifdef CONFIG_KERNEL_FREERTOS
#ifdef CONFIG_CORE_DSP0
extern unsigned long __va_to_pa(unsigned long vaddr);
extern unsigned long __pa_to_va(unsigned long paddr);
#else
#define __va_to_pa(vaddr) ((u32)vaddr)
#define __pa_to_va(vaddr) ((u32)vaddr)
#endif /* CONFIG_CORE_DSP0 */
#elif defined(CONFIG_OS_MELIS)
#include <rtthread.h>

unsigned long awos_arch_virt_to_phys(unsigned long virtaddr);
unsigned long awos_arch_phys_to_virt(unsigned long phyaddr);

#define __va_to_pa(vaddr) awos_arch_virt_to_phys((vaddr))
#define __pa_to_va(paddr) awos_arch_phys_to_virt((paddr))

#else

#define __va_to_pa(vaddr) ((unsigned long)vaddr)
#define __pa_to_va(vaddr) ((unsigned long)vaddr)

#endif /* CONFIG_KERNEL_FREERTOS */

#define PAGE_MEM_ATTR_NON_CACHEABLE 0x000000001UL
#define PAGE_MEM_ATTR_CACHEABLE     0x000000002UL

#ifdef __cplusplus
}
#endif
#endif
