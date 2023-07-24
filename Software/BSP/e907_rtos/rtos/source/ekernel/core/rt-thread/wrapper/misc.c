#include <rtthread.h>
#include <rthw.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <epos.h>
#include <log.h>
#include <ktimer.h>

#include <backtrace.h>

#include <hal_mem.h>
#include <hal_time.h>
#include <hal_interrupt.h>

#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT

#include <sunxi_hal_rtc.h>

#ifndef CONFIG_DYNAMIC_LOG_DEFAULT_LEVEL
#define CONFIG_DYNAMIC_LOG_DEFAULT_LEVEL 5
#endif
static volatile int g_log_level = CONFIG_DYNAMIC_LOG_DEFAULT_LEVEL;
#endif

#ifdef CONFIG_LOG_LEVEL_STORAGE_RTC

static void save_log_level_to_stroage(int level)
{
    unsigned int rtc_log_level = 0;

    level += 1;
    rtc_log_level = hal_rtc_read_data(RTC_LOG_LEVEL_INDEX);
    rtc_log_level &= 0xfffffff0;
    rtc_log_level |= level & 0xf;
    hal_rtc_write_data(RTC_LOG_LEVEL_INDEX, rtc_log_level);
}

static int get_log_level_from_stroage(void)
{
    unsigned int rtc_log_level = 0;

    rtc_log_level = hal_rtc_read_data(RTC_LOG_LEVEL_INDEX);
    if (rtc_log_level & 0xf)
    {
        g_log_level = (rtc_log_level & 0xf) - 1;
    }
    return g_log_level;
}
#elif defined(CONFIG_LOG_LEVEL_STORAGE_NONE)
static void save_log_level_to_stroage(int level)
{
    g_log_level = level;
}
static int get_log_level_from_stroage(void)
{
    return g_log_level;
}
#endif

/*
 * rtc register 5: 0-3bit for log level
 * rtc log level value : 0, rtc default value, no output log level
 * rtc log level value : 1, system log level is 0
 * rtc log level value : 2, system log level is 1
 */
#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT
int get_log_level(void)
{
    int level;
    register unsigned long temp;

    temp = hal_interrupt_save();

    level = get_log_level_from_stroage();

    hal_interrupt_restore(temp);
    return level;
}

void set_log_level(int level)
{
	register unsigned long temp;

    temp = hal_interrupt_save();

    save_log_level_to_stroage(level);
    g_log_level = level;

    hal_interrupt_restore(temp);
}
#endif

extern void *__internal_malloc(uint32_t size);
extern void __internal_free(void *ptr);

static melis_malloc_context_t g_mem_leak_list =
{
    .list       = LIST_HEAD_INIT(g_mem_leak_list.list),
    .ref_cnt    = 0,
    .open_flag  = 0,
    .double_free_check = 0,
};

void epos_memleak_detect_enable(int type)
{
	register unsigned long temp;

    temp = hal_interrupt_save();
    if (g_mem_leak_list.open_flag)
    {
        hal_interrupt_restore(temp);
        return;
    }
    INIT_LIST_HEAD(&g_mem_leak_list.list);
    g_mem_leak_list.ref_cnt     = 1;
    g_mem_leak_list.open_flag   = 1;
    g_mem_leak_list.double_free_check = type;
    hal_interrupt_restore(temp);

    return;
}

void epos_memleak_detect_disable(void)
{
	register unsigned long temp;
    struct list_head        *pos;
    struct list_head        *q;
    uint32_t                count = 0;
    uint32_t                i = 0;

    temp = hal_interrupt_save();

    printf("memory leak nodes:");
    list_for_each_safe(pos, q, &g_mem_leak_list.list)
    {
        melis_heap_buffer_node_t *tmp;
        tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
        printf("\t %03" PRId32 ": ptr = 0x%08lx, size = 0x%08" PRIx32 \
               ", entry = 0x%08" PRIx32 ", thread = %s, tick = %ld.\n", \
               count++, (unsigned long)tmp->vir, (uint32_t)tmp->size, \
               (uint32_t)tmp->entry, tmp->name, tmp->tick);
        for (i = 0; i < CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL; i++)
        {
            if (tmp->caller[i] != NULL)
            {
                printf("            backtrace : 0x%p\n", tmp->caller[i]);
            }
        }
        list_del(pos);
        free(tmp);
    }
    g_mem_leak_list.ref_cnt   = 0;
    g_mem_leak_list.open_flag = 0;
    g_mem_leak_list.double_free_check = 0;

    hal_interrupt_restore(temp);
    return;
}

void epos_memleak_detect_show(void)
{
	register unsigned long temp;
    struct list_head        *pos;
    struct list_head        *q;
    uint32_t                count = 0;
    uint32_t                i = 0;

    temp = hal_interrupt_save();

    printf("memory leak nodes:");
    list_for_each_safe(pos, q, &g_mem_leak_list.list)
    {
        melis_heap_buffer_node_t *tmp;
        tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
        printf("\t %03" PRId32 ": ptr = 0x%08lx, size = 0x%08" PRIx32 \
               ", entry = 0x%08" PRIx32 ", thread = %s, tick = %ld.\n", \
               count++, (unsigned long)tmp->vir, (uint32_t)tmp->size, \
               (uint32_t)tmp->entry, tmp->name, tmp->tick);
        for (i = 0; i < CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL; i++)
        {
            if (tmp->caller[i] != NULL)
            {
                printf("           backtrace : 0x%p\n", tmp->caller[i]);
            }
        }
    }

    hal_interrupt_restore(temp);
    return;
}

void epos_memleak_detect_search(unsigned long addr)
{
	register unsigned long temp;
    struct list_head        *pos;
    struct list_head        *q;
    uint32_t                count = 0;
    uint32_t                i = 0;

    temp = hal_interrupt_save();

    list_for_each_safe(pos, q, &g_mem_leak_list.list)
    {
        melis_heap_buffer_node_t *tmp;
        tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
        if ((addr < ((unsigned long)tmp->vir + tmp->size))
		&& (addr >= (unsigned long)tmp->vir))
        {
            printf("\t %03" PRId32 ": ptr = 0x%08lx, size = 0x%08" PRIx32 \
                   ", entry = 0x%08" PRIx32 ", thread = %s, tick = %ld.\n", \
                   count++, (unsigned long)tmp->vir, (uint32_t)tmp->size, \
                   (uint32_t)tmp->entry, tmp->name, tmp->tick);
            for (i = 0; i < CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL; i++)
            {
                if (tmp->caller[i] != NULL)
                {
                    printf("           backtrace : 0x%p\n", tmp->caller[i]);
                }
            }
        }
    }

    hal_interrupt_restore(temp);
    return;
}

void *rt_malloc(rt_size_t size)
{
    void *ptr;
    unsigned long sp;
	register unsigned long temp;

    melis_heap_buffer_node_t *new = NULL;

#if 0
    /* the usb driver need to malloc memory in ISR handler */
    if (hal_interrupt_get_nest() != 0)
    {
        __err("fatal error.");
        hal_sys_abort();
    }
#endif

    ptr = __internal_malloc(size);
    if (ptr == RT_NULL)
    {
        return RT_NULL;
    }

    if (g_mem_leak_list.open_flag)
    {
        sp = (unsigned long)&sp;
        new = __internal_malloc(sizeof(melis_heap_buffer_node_t));
        if (new == RT_NULL)
        {
            goto RTN;
        }

        memset(new, 0x00, sizeof(melis_heap_buffer_node_t));

        INIT_LIST_HEAD(&new->i_list);

        new->size = size;
        new->vir = ptr;
        new->entry = 0;
        new->name = "thread";
        new->tick = hal_tick_get();
        new->stk = (void *)sp;

#ifdef CONFIG_DEBUG_BACKTRACE
        backtrace(NULL, new->caller, CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL, 3, NULL);
#endif
        temp = hal_interrupt_save();
        list_add(&new->i_list, &g_mem_leak_list.list);
        hal_interrupt_restore(temp);
    }
RTN:
    return ptr;
}

void rt_free(void *ptr)
{
    struct list_head       *pos;
    struct list_head       *q;
    melis_heap_buffer_node_t *tmp;
    melis_heap_buffer_node_t *node = NULL;
    uint32_t i = 0;

    if (ptr == RT_NULL)
    {
        return;
    }

#if 0
    /* the usb driver need to free memory in ISR handler */
    if (hal_interrupt_get_nest() != 0)
    {
        __err("fatal error.");
        hal_sys_abort();
    }
#endif

    if (g_mem_leak_list.open_flag)
    {
	    register unsigned long temp;

        temp = hal_interrupt_save();

        list_for_each_safe(pos, q, &g_mem_leak_list.list)
        {
            tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
            if (tmp->vir == ptr)
            {
                list_del(pos);
                node = tmp;
                break;
            }
        }

        hal_interrupt_restore(temp);
    }

    if (node)
    {
        __internal_free(node);
    }
#ifdef CONFIG_DOUBLE_FREE_CHECK
    else if(g_mem_leak_list.double_free_check)
    {
        printf("may be meet double free: chunk=0x%08lx\r\n", (unsigned long)ptr);
#ifdef CONFIG_DEBUG_BACKTRACE
        printf("now free callstack:\r\n");
        backtrace(NULL, NULL, 0, 0, printf);
#endif
    }
#endif
    __internal_free(ptr);

    return;
}

int printk(const char *fmt, ...)
{
    va_list args;
    rt_size_t length;
    static char printk_log_buf[RT_CONSOLEBUF_SIZE];
#ifdef CONFIG_PRINT_TIMESTAMP
    static char printk_time_buf[16];
#endif
    char *p;
    int log_level;
    register rt_base_t level;

    va_start(args, fmt);
    level = rt_hw_interrupt_disable();
    rt_enter_critical();

#ifdef CONFIG_PRINT_TIMESTAMP
    rt_snprintf(printk_time_buf, sizeof(printk_time_buf) - 1, "[%d.%3d]",
                  (int)(ktime_get() / 1000000), (int)((ktime_get() %1000000) / 1000));
#endif
    length = rt_vsnprintf(printk_log_buf, sizeof(printk_log_buf) - 1, fmt, args);
    if (length > RT_CONSOLEBUF_SIZE - 1)
    {
        length = RT_CONSOLEBUF_SIZE - 1;
    }

#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT
    log_level = get_log_level();
    p = (char *)&printk_log_buf;
    if (p[0] != '<' || p[1] < '0' || p[1] > '7' || p[2] != '>')
    {
#ifdef CONFIG_VIRT_LOG
        extern void virt_log_put_buf(char *buf);
#ifdef CONFIG_PRINT_TIMESTAMP
        virt_log_put_buf(printk_time_buf);
#endif
        virt_log_put_buf(printk_log_buf);
#endif

#ifdef CONFIG_AMP_TRACE_SUPPORT
        extern void amp_log_put_str(char *buf);
#ifdef CONFIG_PRINT_TIMESTAMP
        amp_log_put_str(printk_time_buf);
#endif
        amp_log_put_str(printk_log_buf);
#endif

        if (log_level > (OPTION_LOG_LEVEL_CLOSE))
        {
#ifdef CONFIG_PRINT_TIMESTAMP
            rt_kprintf("%s", printk_time_buf);
#endif
            rt_kprintf("%s", (char *)&printk_log_buf);
        }
    }
    else
    {
#ifdef CONFIG_VIRT_LOG
        extern void virt_log_put_buf(char *buf);
#ifdef CONFIG_PRINT_TIMESTAMP
        virt_log_put_buf(printk_time_buf);
#endif
        virt_log_put_buf(&printk_log_buf[3]);
#endif

#ifdef CONFIG_AMP_TRACE_SUPPORT
        extern void amp_log_put_str(char *buf);
#ifdef CONFIG_PRINT_TIMESTAMP
        amp_log_put_str(printk_time_buf);
#endif
        amp_log_put_str(&printk_log_buf[3]);
#endif

        if (log_level >= (p[1] - '0'))
        {
#ifdef CONFIG_PRINT_TIMESTAMP
            rt_kprintf("%s", printk_time_buf);
#endif
            rt_kprintf("%s", (char *)&printk_log_buf[3]);
        }
    }
#else
#ifdef CONFIG_PRINT_TIMESTAMP
    rt_kprintf("%s", printk_time_buf);
#endif
    rt_kprintf("%s", (char *)&printk_log_buf);
#endif
    rt_exit_critical();
    rt_hw_interrupt_enable(level);
    va_end(args);

    return length;
}

void dump_system_information(void)
{
    struct rt_object_information *information;
    struct rt_object *object;
    struct rt_list_node *node;
    rt_thread_t temp;
    rt_tick_t  duration;
    rt_uint32_t total, used, max_used;
    rt_uint8_t *ptr;
    char *stat;
    rt_ubase_t stk_free;

    rt_enter_critical();
    printk("\r\n");
    printk("    -----------------------------------------------TSK Usage Report----------------------------------------------------------\r\n");
    printk("        name     errno    entry       stat   prio     tcb     slice stacksize      stkfree  lt    si   so       stack_range  \r\n");

    information = rt_object_get_information(RT_Object_Class_Thread);
    RT_ASSERT(information != RT_NULL);
    for (node = information->object_list.next; node != &(information->object_list); node = node->next)
    {
        rt_uint8_t status;
        rt_uint32_t pc = 0xdeadbeef;

        object = rt_list_entry(node, struct rt_object, list);
        temp = (rt_thread_t)object;


        status = (temp->stat & RT_THREAD_STAT_MASK);

        if (status == RT_THREAD_READY)
        {
            stat = "running";
        }
        else if (status == RT_THREAD_SUSPEND)
        {
            stat = "suspend";
        }
        else if (status == RT_THREAD_INIT)
        {
            stat = "initing";
        }
        else if (status == RT_THREAD_CLOSE)
        {
            stat = "closing";
        }
        else
        {
            stat = "unknown";
        }

        ptr = (rt_uint8_t *)temp->stack_addr;
        while (*ptr == '#')
        {
            ptr ++;
        }

        stk_free = (rt_ubase_t) ptr - (rt_ubase_t) temp->stack_addr;
        printk("%15s%5ld   0x%lx  %9s %4d   0x%lx  %3ld %8d    %8ld    %02ld  %04d %04d  [0x%lx-0x%lx]\r\n", \
                   temp->name,
                   temp->error,
                   (rt_ubase_t)temp->entry,
                   stat,
                   temp->current_priority,
                   (rt_ubase_t)temp,
                   temp->init_tick,
                   temp->stack_size,
                   stk_free,
                   temp->remaining_tick, temp->sched_i, temp->sched_o, (rt_ubase_t)temp->stack_addr, (rt_ubase_t)temp->stack_addr + temp->stack_size);
    }

    printk("    -------------------------------------------------------------------------------------------------------------------------\r\n");
    rt_memory_info(&total, &used, &max_used);
    printk("\n\r    memory info:\n\r");
    printk("\tTotal  0x%08x\n\r" \
               "\tUsed   0x%08x\n\r" \
               "\tMax    0x%08x\n\r", \
               total, used, max_used);
    printk("    ------------------------------------------------memory information-------------------------------------------------------\r\n");

    rt_exit_critical();

    return;
}

void show_thread_info(void *thread)
{
    rt_thread_t th = (rt_thread_t) thread;
    printk("thread: %s, entry: 0x%p, stack_base: 0x%p,stack_size: 0x%08x.\r\n", \
               th->name, \
               th->entry, \
               th->stack_addr, \
               th->stack_size);
}

#ifdef CONFIG_DMA_COHERENT_HEAP

static struct rt_memheap coherent;

int dma_coherent_heap_init(void)
{
    int ret = -1;
    void *heap_start;

    heap_start = dma_alloc_coherent_non_cacheable(CONFIG_DMA_COHERENT_HEAP_SIZE);
    if (!heap_start)
    {
        printf("alloc non cacheable buffer failed!\n");
        return -1;
    }
    return rt_memheap_init(&coherent, "dma-coherent", heap_start, CONFIG_DMA_COHERENT_HEAP_SIZE);
}

void *dma_coherent_heap_alloc(size_t size)
{
    return rt_memheap_alloc(&coherent, size);
}

void dma_coherent_heap_free(void *ptr)
{
    rt_memheap_free(ptr);
}

void *dma_coherent_heap_alloc_align(size_t size,int align)
{
    void *fake_ptr = NULL;
    void *malloc_ptr = NULL;

    malloc_ptr = dma_coherent_heap_alloc(size + align);
    if ((unsigned long)malloc_ptr & (sizeof(long) - 1))
    {
        printf("error: mm_alloc not align. \r\n");
        return NULL;
    }

    if (!malloc_ptr)
    {
        return NULL;
    }

    fake_ptr = (void *)((unsigned long)(malloc_ptr + align) & (~(align - 1)));
    *(unsigned long *)((unsigned long *)fake_ptr - 1) = (unsigned long)malloc_ptr;

    return fake_ptr;
}

void dma_coherent_heap_free_align(void *ptr)
{
    void *malloc_ptr = NULL;
    if (!ptr)
    {
        return;
    }
    malloc_ptr = (void *) * (unsigned long *)((unsigned long *)ptr - 1);
    rt_memheap_free(malloc_ptr);
}

#endif

void *k_malloc(rt_size_t sz) __attribute__((alias("rt_malloc")));
void k_free(void *ptr) __attribute__((alias("rt_free")));
