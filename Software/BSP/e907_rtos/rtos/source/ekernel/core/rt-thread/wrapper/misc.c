#include <rtthread.h>
#include <rthw.h>

#include <stdio.h>
#include <epos.h>
#include <log.h>

#include <backtrace.h>

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
    register rt_base_t temp;

    temp = rt_hw_interrupt_disable();

    level = get_log_level_from_stroage();

    rt_hw_interrupt_enable(temp);
    return level;
}

void set_log_level(int level)
{
    register rt_base_t temp;

    temp = rt_hw_interrupt_disable();

    save_log_level_to_stroage(level);
    g_log_level = level;

    rt_hw_interrupt_enable(temp);
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
    register rt_base_t  temp;

    temp = rt_hw_interrupt_disable();
    if (g_mem_leak_list.open_flag)
    {
        rt_hw_interrupt_enable(temp);
        return;
    }
    INIT_LIST_HEAD(&g_mem_leak_list.list);
    g_mem_leak_list.ref_cnt     = 1;
    g_mem_leak_list.open_flag   = 1;
    g_mem_leak_list.double_free_check = type;
    rt_hw_interrupt_enable(temp);

    return;
}

void epos_memleak_detect_disable(void)
{
    register rt_base_t      cpu_sr;
    struct list_head        *pos;
    struct list_head        *q;
    uint32_t                count = 0;
    uint32_t                i = 0;

    cpu_sr = rt_hw_interrupt_disable();

    __log("memory leak nodes:");
    list_for_each_safe(pos, q, &g_mem_leak_list.list)
    {
        melis_heap_buffer_node_t *tmp;
        tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
        rt_kprintf("\t %03d: ptr = 0x%08x, size = 0x%08x, entry = 0x%08x, thread = %s, tick = %d.\n", \
                   count ++, (unsigned long)tmp->vir, (uint32_t)tmp->size, (uint32_t)tmp->entry, tmp->name, tmp->tick);
        for (i = 0; i < CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL; i++)
        {
            if (tmp->caller[i] != NULL)
            {
                rt_kprintf("            backtrace : 0x%p\n", tmp->caller[i]);
            }
        }
        list_del(pos);
        rt_free(tmp);
    }
    g_mem_leak_list.ref_cnt   = 0;
    g_mem_leak_list.open_flag = 0;
    g_mem_leak_list.double_free_check = 0;

    rt_hw_interrupt_enable(cpu_sr);
    return;
}

void epos_memleak_detect_show(void)
{
    register rt_base_t      cpu_sr;
    struct list_head        *pos;
    struct list_head        *q;
    uint32_t                count = 0;
    uint32_t                i = 0;

    cpu_sr = rt_hw_interrupt_disable();

    __log("memory leak nodes:");
    list_for_each_safe(pos, q, &g_mem_leak_list.list)
    {
        melis_heap_buffer_node_t *tmp;
        tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
        rt_kprintf("\t %03d: ptr = 0x%08x, size = 0x%08x, entry = 0x%08x, thread = %s, tick = %d.\n", \
                   count ++, (unsigned long)tmp->vir, (uint32_t)tmp->size, (uint32_t)tmp->entry, tmp->name, tmp->tick);
        for (i = 0; i < CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL; i++)
        {
            if (tmp->caller[i] != NULL)
            {
                rt_kprintf("           backtrace : 0x%p\n", tmp->caller[i]);
            }
        }
    }

    rt_hw_interrupt_enable(cpu_sr);
    return;
}

void epos_memleak_detect_search(unsigned long addr)
{
    register rt_base_t      cpu_sr;
    struct list_head        *pos;
    struct list_head        *q;
    uint32_t                count = 0;
    uint32_t                i = 0;

    cpu_sr = rt_hw_interrupt_disable();

    list_for_each_safe(pos, q, &g_mem_leak_list.list)
    {
        melis_heap_buffer_node_t *tmp;
        tmp = list_entry(pos, melis_heap_buffer_node_t, i_list);
        if ((addr < ((unsigned long)tmp->vir + tmp->size))
		&& (addr >= (unsigned long)tmp->vir))
        {
            rt_kprintf("\t %03d: ptr = 0x%08x, size = 0x%08x, entry = 0x%08x, thread = %s, tick = %d.\n", \
                   count ++, (unsigned long)tmp->vir, (uint32_t)tmp->size, (uint32_t)tmp->entry, tmp->name, tmp->tick);
            for (i = 0; i < CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL; i++)
            {
                if (tmp->caller[i] != NULL)
                {
                    rt_kprintf("           backtrace : 0x%p\n", tmp->caller[i]);
                }
            }
        }
    }

    rt_hw_interrupt_enable(cpu_sr);
    return;
}


void *rt_malloc(rt_size_t size)
{
    void *ptr;
    unsigned long sp;

    melis_heap_buffer_node_t *new = NULL;

    if (rt_interrupt_get_nest() != 0)
    {
        __err("fatal error.");
        software_break();
    }

    ptr = __internal_malloc(size);
    if (ptr == RT_NULL)
    {
        return RT_NULL;
    }

    if (g_mem_leak_list.open_flag)
    {
        sp = (unsigned long)&sp;
        rt_enter_critical();
        new = __internal_malloc(sizeof(melis_heap_buffer_node_t));
        if (new == RT_NULL)
        {
            rt_exit_critical();
            goto RTN;
        }

        rt_memset(new, 0x00, sizeof(melis_heap_buffer_node_t));

        INIT_LIST_HEAD(&new->i_list);

        new->size = size;
        new->vir = ptr;
        if (rt_thread_self() == NULL)
        {
            new->entry = 0;
            new->name = "unknown";
        }
        else
        {
            new->entry = (unsigned long)rt_thread_self()->entry;
            new->name = (__s8 *)rt_thread_self()->name;
        }
        new->tick = rt_tick_get();
        new->stk = (void *)sp;

#ifdef CONFIG_DEBUG_BACKTRACE
        backtrace(NULL, new->caller, CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL, 3, NULL);
#endif

        list_add(&new->i_list, &g_mem_leak_list.list);

        rt_exit_critical();
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
    if (rt_interrupt_get_nest() != 0)
    {
        __err("fatal error.");
        software_break();
    }
#endif

    if (g_mem_leak_list.open_flag)
    {
        rt_enter_critical();

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

        rt_exit_critical();
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
    char *p;
    int log_level;
    register rt_base_t level;

    va_start(args, fmt);
    level = rt_hw_interrupt_disable();
    rt_enter_critical();

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
        if (log_level > (OPTION_LOG_LEVEL_CLOSE))
        {
            rt_kprintf("%s", (char *)&printk_log_buf);
        }
    }
    else
    {
        if (log_level >= (p[1] - '0'))
        {
            rt_kprintf("%s", (char *)&printk_log_buf[3]);
        }
    }
#else
    rt_kprintf("%s", (char *)&printk_log_buf);
#endif
    rt_exit_critical();
    rt_hw_interrupt_enable(level);
    va_end(args);

    return length;
}

void *k_malloc(rt_size_t sz) __attribute__((alias("rt_malloc")));
void k_free(void *ptr) __attribute__((alias("rt_free")));
