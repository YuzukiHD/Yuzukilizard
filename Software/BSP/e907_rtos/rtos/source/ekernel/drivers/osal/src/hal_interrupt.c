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
#include <interrupt.h>
#include <rtthread.h>
#include <kconfig.h>
#include <rthw.h>

#if defined(CONFIG_KERNEL_FREERTOS)
#include <compiler.h>
#endif

extern const void *free_irq(uint32_t);
extern void disable_irq(uint32_t irq);
extern void enable_irq(uint32_t irq);

int32_t hal_request_irq(int32_t irq, hal_irq_handler_t handler, const char *name, void *data)
{
    return request_irq(irq, handler, 0, name, data);
}

void hal_free_irq(int32_t irq)
{
	free_irq(irq);
}

int hal_enable_irq(int32_t irq)
{
	enable_irq(irq);

	return 0;
}

void hal_disable_irq(int32_t irq)
{
	disable_irq(irq);
}

uint32_t hal_interrupt_get_nest(void)
{
    uint32_t nest = rt_interrupt_get_nest();
    return nest;
}

void hal_interrupt_enable(void)
{
    void local_irq_enable(void);
    local_irq_enable();
}

void hal_interrupt_disable(void)
{
    void local_irq_disable(void);
    local_irq_disable();
}

unsigned long hal_interrupt_save(void)
{
    return rt_hw_interrupt_disable();
}

void hal_interrupt_restore(unsigned long flag)
{
    return rt_hw_interrupt_enable(flag);
}
