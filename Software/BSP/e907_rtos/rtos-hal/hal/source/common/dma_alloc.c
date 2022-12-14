#include <stdio.h>

#include <hal_mem.h>
#include <hal_atomic.h>
#include <aw_list.h>
#include "sunxi_hal_common.h"

#ifdef CONFIG_OS_MELIS
#ifndef CONFIG_DMA_VMAREA_START_ADDRESS
#define CONFIG_DMA_VMAREA_START_ADDRESS (0x80000000)
#endif

#define PAGE_SIZE (4096)

struct vm_struct
{
    void *page;
    int size;
    unsigned long prot;
    struct list_head i_list;
};

void unmap_vmap_area(unsigned long vmaddr, unsigned long free_sz, void *page);
int map_vm_area(unsigned long size, unsigned long prot, void *pages[], unsigned long target_vmaddr);

static LIST_HEAD(g_vm_struct_list);
static hal_spinlock_t dma_alloc_coherent_lock;
#endif

void dma_free_coherent(void *addr)
{
    void *malloc_ptr = NULL;
    if (!addr)
    {
        return;
    }
    malloc_ptr = (void *) * (unsigned long *)((unsigned long *)addr - 1);
    hal_free(malloc_ptr);
}

void *dma_alloc_coherent(size_t size)
{
    void *fake_ptr = NULL;
    void *malloc_ptr = NULL;

    malloc_ptr = hal_malloc(size + 2 * CACHELINE_LEN);
    if ((unsigned long)malloc_ptr & (sizeof(long) - 1))
    {
        printf("error: mm_alloc not align. \r\n");
        return NULL;
    }

    if (!malloc_ptr)
    {
        return NULL;
    }

    fake_ptr = (void *)((unsigned long)(malloc_ptr + CACHELINE_LEN) & (~(CACHELINE_LEN - 1)));
    *(unsigned long *)((unsigned long *)fake_ptr - 1) = (unsigned long)malloc_ptr;

    return fake_ptr;
}

#ifdef CONFIG_OS_MELIS

void dma_alloc_coherent_init(void)
{
    hal_spin_lock_init(&dma_alloc_coherent_lock);
}

void dma_free_coherent_prot(void *addr)
{
    struct vm_struct *tmp;
    struct list_head *pos;
    struct list_head *q;
    char *malloc_ptr;
    unsigned long cpsr;

    if (!addr)
    {
        return;
    }

    malloc_ptr = (char *)addr - CONFIG_DMA_VMAREA_START_ADDRESS;
    hal_free_align(malloc_ptr);

    cpsr = hal_spin_lock_irqsave(&dma_alloc_coherent_lock);
    list_for_each_safe(pos, q, &g_vm_struct_list)
    {
        tmp = list_entry(pos, struct vm_struct, i_list);
        if (addr == tmp->page)
        {
            list_del(pos);
            hal_spin_unlock_irqrestore(&dma_alloc_coherent_lock, cpsr);
            unmap_vmap_area((unsigned long)addr, tmp->size, tmp->page);
            hal_free(tmp);
            return;
        }
    }
    hal_spin_unlock_irqrestore(&dma_alloc_coherent_lock, cpsr);
}

void *dma_alloc_coherent_prot(size_t size, unsigned long prot)
{
    char *malloc_ptr = NULL;
    unsigned long *pages = NULL;
    struct vm_struct *area;
    int page_num = 0;
    uint32_t cpsr;
    int i;

    size = ALIGN_UP(size, PAGE_SIZE);
    page_num = size / PAGE_SIZE;

    area = hal_malloc(sizeof(struct vm_struct));
    if (!area)
    {
        return NULL;
    }

    pages = hal_malloc(sizeof(void *) * page_num);
    if (!pages)
    {
        hal_free(area);
        return NULL;
    }

    malloc_ptr = hal_malloc_align(size, PAGE_SIZE);
    if (!malloc_ptr)
    {
        hal_free(pages);
        hal_free(area);
        return NULL;
    }

    for (i = 0; i < page_num; i++)
    {
        pages[i] = __va_to_pa((unsigned long)malloc_ptr + i * PAGE_SIZE);
    }

    area->page = malloc_ptr + CONFIG_DMA_VMAREA_START_ADDRESS;
    area->size = size;
    area->prot = prot;

    cpsr = hal_spin_lock_irqsave(&dma_alloc_coherent_lock);
    INIT_LIST_HEAD(&area->i_list);
    list_add(&area->i_list, &g_vm_struct_list);
    hal_spin_unlock_irqrestore(&dma_alloc_coherent_lock, cpsr);

    if (map_vm_area(size, get_memory_prot(prot), (void *)pages, (unsigned long)malloc_ptr + CONFIG_DMA_VMAREA_START_ADDRESS))
    {
        hal_free_align(malloc_ptr);
        hal_free(pages);
        hal_free(area);
        return NULL;
    }

    hal_free(pages);
    return malloc_ptr + CONFIG_DMA_VMAREA_START_ADDRESS;
}

void *dma_alloc_coherent_non_cacheable(size_t size)
{
    return dma_alloc_coherent_prot(size, PAGE_MEM_ATTR_NON_CACHEABLE);
}

void dma_free_coherent_non_cacheable(void *addr)
{
    dma_free_coherent_prot(addr);
}
#endif
