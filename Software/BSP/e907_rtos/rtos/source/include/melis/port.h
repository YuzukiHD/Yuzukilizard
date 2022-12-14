#ifndef __RTT_PORT_H__
#define __RTT_PORT_H__

#include <typedef.h>

typedef int32_t (*pfun_bsp_init_t)(void);
typedef void (*pfun_krnl_init_t)(void *);

__hdle awos_task_create(const char *name, void (*entry)(void *parameter), \
                        void *parameter, uint32_t stack_size, uint8_t priority, \
                        uint32_t tick);

int32_t awos_task_delete(__hdle thread);
__hdle awos_task_self(void);

void awos_init_bootstrap(pfun_bsp_init_t rt_system_bsp_init, pfun_krnl_init_t kernel_init, uint64_t ticks);

void *k_malloc_align(uint32_t size, uint32_t align);
void k_free_align(void *ptr, uint32_t size);
void *k_malloc(uint32_t size);
void k_free(void *ptr);

int sprintf(char *buf, const char *format, ...);
int vsprintf(char *buf, const char *format, va_list arg_ptr);

void *k_calloc(uint32_t count, uint32_t size);
void *k_realloc(void *mem_address, uint32_t newsize);

#endif

