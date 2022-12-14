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
#include <stdlib.h>
#include <rtthread.h>
#include <port.h>
#include <excep.h>
#include <typedef.h>
#include <rtdef.h>
#include <compiler.h>
#include <hal_interrupt.h>

__hdle awos_task_create(const char *name, void (*entry)(void *parameter), \
                        void *parameter, uint32_t stack_size, uint8_t priority, \
                        uint32_t tick)
{
    rt_thread_t thr;

    thr = rt_thread_create(name, entry, parameter, stack_size, priority, tick);

    RT_ASSERT(thr != RT_NULL);
    rt_thread_startup(thr);

    return (__hdle)thr;
}

int32_t awos_task_delete(__hdle thread)
{
    RT_ASSERT(thread != RT_NULL);
    if (thread == rt_thread_self())
    {
        void rt_thread_exit(void);
        rt_thread_exit();
        CODE_UNREACHABLE;
    }
    else
    {
        rt_thread_delete((rt_thread_t)thread);
    }

    return 0;
}

__hdle awos_task_self(void)
{
    return (__hdle)rt_thread_self();
}

void awos_arch_irq_trap_enter(void)
{
    rt_interrupt_enter();
}

void awos_arch_irq_trap_leave(void)
{
    rt_interrupt_leave();
}

void awos_arch_tick_increase(void)
{
    rt_tick_increase();
}

uint8_t awos_arch_isin_irq(void)
{
    return hal_interrupt_get_nest() ?  1 : 0;
}

void *k_malloc_align(uint32_t size, uint32_t align)
{
    return rt_malloc_align(size, align);
}

void k_free_align(void *ptr, uint32_t size)
{
    return rt_free_align(ptr);
}
