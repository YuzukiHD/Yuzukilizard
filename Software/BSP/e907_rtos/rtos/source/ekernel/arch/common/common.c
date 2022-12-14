/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <_ansi.h>
#include <reent.h>
#include <stdio.h>
#include <stdarg.h>
#include <version.h>

#include <rtthread.h>

#include <log.h>
#include <hal_interrupt.h>
#include <hal_mem.h>

int _printf_r(struct _reent *ptr,
              const char *__restrict fmt, ...)
{
    int ret;
    va_list ap;

    _REENT_SMALL_CHECK_INIT(ptr);
    va_start(ap, fmt);
    ret = _vfprintf_r(ptr, _stdout_r(ptr), fmt, ap);
    va_end(ap);
    return ret;
}

#ifdef _NANO_FORMATTED_IO
int
_iprintf_r(struct _reent *, const char *, ...)
_ATTRIBUTE((__alias__("_printf_r")));
#endif

#ifndef _REENT_ONLY

void enter_io_critical(void)
{
    if (!hal_interrupt_get_nest())
    {
        rt_enter_critical();
    }
}

void exit_io_critical(void)
{
    if (!hal_interrupt_get_nest())
    {
        rt_exit_critical();
    }
}

int printf(const char *__restrict fmt, ...)
{
    int ret = 0;

    enter_io_critical();

    va_list ap;
    struct _reent *ptr = _REENT;

    _REENT_SMALL_CHECK_INIT(ptr);
    va_start(ap, fmt);
    ret = _vfprintf_r(ptr, _stdout_r(ptr), fmt, ap);
    va_end(ap);

    exit_io_critical();

    return ret;
}

#ifdef _NANO_FORMATTED_IO
int
iprintf(const char *, ...)
_ATTRIBUTE((__alias__("printf")));
#endif
#endif /* ! _REENT_ONLY */

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
    rt_kprintf("\r\n");
    rt_kprintf("    -----------------------------------------------TSK Usage Report----------------------------------------------------------\r\n");
    rt_kprintf("        name     errno    entry       stat   prio     tcb     slice stacksize      stkfree  lt    si   so       stack_range  \r\n");

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
        rt_kprintf("%15s%5ld   0x%lx  %9s %4d   0x%lx  %3ld %8d    %8ld    %02ld  %04d %04d  [0x%lx-0x%lx]\r\n", \
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

    rt_kprintf("    -------------------------------------------------------------------------------------------------------------------------\r\n");
    rt_memory_info(&total, &used, &max_used);
    rt_kprintf("\n\r    memory info:\n\r");
    rt_kprintf("\tTotal  0x%08x\n\r" \
               "\tUsed   0x%08x\n\r" \
               "\tMax    0x%08x\n\r", \
               total, used, max_used);
    rt_kprintf("    ------------------------------------------------memory information-------------------------------------------------------\r\n");

    rt_exit_critical();

    return;
}

void dump_memory(uint32_t *buf, int32_t len)
{
    int i;

    for (i = 0; i < len; i ++)
    {
        if ((i % 4) == 0)
        {
            rt_kprintf("\n\r0x%p: ", buf + i);
        }
        rt_kprintf("0x%08x ", buf[i]);
    }
    rt_kprintf("\n\r");

    return;
}

void dump_register_memory(char *name, unsigned long addr, int32_t len)
{
    int i;
    int32_t check_virtual_address(unsigned long vaddr);

    if (check_virtual_address(addr) && check_virtual_address(addr + len * sizeof(int)))
    {
        rt_kprintf("\n\rdump %s memory:", name);
        dump_memory((uint32_t *)addr, len);
    }
    else
    {
        rt_kprintf("\n\r%s register corrupted!!\n", name);
    }

    return;
}

int g_cli_direct_read = 0;

void panic_goto_cli(void)
{
    if (g_cli_direct_read > 0)
    {
        rt_kprintf("%s can not be reentrant!\n", __func__);
        return;
    }
    g_cli_direct_read = 1;
#ifdef CONFIG_RT_USING_FINSH
    void finsh_thread_entry(void *parameter);
    finsh_thread_entry(NULL);
#endif
}

#define MELIS_VERSION_STRING(major, minor, patch) \
    #major"."#minor"."#patch

const char *melis_banner_string(void)
{
    return ("V"MELIS_VERSION_STRING(3, 9, 0));
}

static const char melis_banner [] =
{
    "|                  /'\\_/`\\       (_ )  _                      ( )( )     /' _`\\    /' _`\\ \n\r"
    "|                  |     |   __   | | (_)  ___  ______  _   _ | || |     | ( ) |   | ( ) |    \n\r"
    "|                  | (_) | /'__`\\ | | | |/',__)(______)( ) ( )| || |_    | | | |   | | | |   \n\r"
    "|                  | | | |(  ___/ | | | |\\__, \\        | \\_/ |(__ ,__) _ | (_) | _ | (_) | \n\r"
    "|                  (_) (_)`\\____)(___)(_)(____/        `\\___/'   (_)  (_)`\\___/'(_)`\\___/'\n\r"
};

#if (CONFIG_LOG_DEFAULT_LEVEL == 0)
#define printk(...)
#endif

void show_melis_version(void)
{
#ifdef CONFIG_SHOW_FULL_VERSION
    //use newlibc printk for long strings.
    printk("\n\r");
    printk("===============================================================================================================\n\r");
    printk("%s", melis_banner);
    printk("|version : %s%s%s\n\r", melis_banner_string(), XPOSTO(110), "|");
    printk("|commitid: %.*s%s%s\n\r", 100, SDK_GIT_VERSION, XPOSTO(110), "|");
#if defined(HAL_GIT_VERSION)
    printk("|halgitid: %.*s%s%s\n\r", 100, HAL_GIT_VERSION, XPOSTO(110), "|");
#endif
    printk("|sunxiver: %x%s%s\n\r", MELIS_VERSION_CODE, XPOSTO(110), "|");
    printk("|timever : %.*s%s%s\n\r", 100, SDK_UTS_VERSION, XPOSTO(110), "|");
    printk("|compiler: %.*s%s%s\n\r", 100, GCC_VERSION, XPOSTO(110), "|");
#ifdef CONFIG_CC_OPTIMIZE_FOR_SIZE
    printk("|optimal : %.*s%s%s\n\r", 100, "-Os -g -gdwarf-2 -gstrict-dwarf", XPOSTO(110), "|");
#else
    printk("|optimal : %.*s%d%s%s%s\n\r", 100, "-O", CONFIG_CC_OPTIMIZE_LEVEL, " -g -gdwarf-2 -gstrict-dwarf", XPOSTO(110), "|");
#endif
    printk("|linker  : %.*s%s%s\n\r", 100, LNK_VERSION, XPOSTO(110), "|");
    printk("|newlibc : %.*s%s%s\n\r", 100, _NEWLIB_VERSION, XPOSTO(110), "|");
    printk("|author  : %.*s%s%s\n\r", 100, SDK_COMPILE_BY, XPOSTO(110), "|");
    printk("===============================================================================================================\n\r");
#else
    printk("\n\r");
    /* printk("================melis  info================\n\r"); */
    printk("|commitid: %.*s\n\r", 40, SDK_GIT_SHORT_VERSION);
    printk("|halgitid: %.*s\n\r", 40, HAL_GIT_SHORT_VERSION);
    printk("|timever : %.*s\n\r", 40, SDK_UTS_VERSION);
    /* printk("===========================================\n\r"); */
#endif
    printk("\n\r");
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
