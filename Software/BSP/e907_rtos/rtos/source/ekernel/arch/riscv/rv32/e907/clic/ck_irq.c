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
#include <stdint.h>
#include <stdio.h>
#include <soc.h>
#include <core_rv32.h>
#include <hal_interrupt.h>
#include <interrupt.h>
#include <excep.h>
#include "csi_rv32_gcc.h"
#include <inttypes.h>

extern void Default_Handler(void);
extern void SysTick_Handler(void);

void (*g_irqvector[208])(void);
void (*g_nmivector)(void);
#ifdef CONFIG_STANDBY
uint8_t g_irq_status[208] = {0};
#endif

typedef void (*irq_flow_handler_t)(int irq, void *data);

#define CLIC_PERIPH_IRQ_OFFSET (16)
#define E907_SUNXI_IRQ_MAX (192)
#define CLIC_IRQ_NUM (E907_SUNXI_IRQ_MAX + CLIC_PERIPH_IRQ_OFFSET)

static const char *irq_string[CLIC_IRQ_NUM] = {
	[0 ... 2] = NULL,
	"Machine_Software",
	"User_Timer",
	"Supervisor_Timer",
	NULL,
	"CORET",
	[8 ... 10] = NULL,
	"Machine_External",
	[12 ... 15] = NULL,
	"CPUX_MBOX_RX",     /* CPUX MSGBOX READ IRQ FOR CPUX */
	"CPUX_MBOX_TX",     /* CPUX MSGBOX READ IRQ FOR E907 */
	"UART0",
	"UART1",
	"UART2",
	"UART3",
	[22 ... 24] = NULL,
	"TWI0",
	"TWI1",
	"TWI2",
	"TWI3",
	"TWI4",
	NULL,
	"SPI0",
	"SPI1",
	"SPI2",
	"PWM",
	"SPI_FLASH",
	"WIEGAND",
	"SPI3",
	[38 ... 39] = NULL,
	"DMIC",
	"AUDIO_CODEC",
	"I2S_PCM0",
	"I2S_PCM1",
	NULL,
	"USB_OTG",
	"USB_EHCI",
	"USB_OHCI",
	[48 ... 55] = NULL,
	"SMHC0",
	"SMHC1",
	"SMHC2",
	"MSI",
	"SMC",
	NULL,
	"GMAC",
	NULL,
	"CCU_FERR",
	"AHB_HREADY_TIME_OUT",
	"DMA_CPUX_NS",
	"DMA_CPUX_S",
	"CE_NS",
	"CE_S",
	"SPINLOCK",
	"HSTIMER0",
	"HSTIMER1",
	"GPADC",
	"THS",
	"TIMER0",
	"TIMER1",
	"TIMER2",
	"TIMER3",
	"WDG",
	"IOMMU",
	"NPU",
	"VE",
	"GPIOA_NS",
	"GPIOA_S",
	[85 ... 86] = NULL,
	"GPIOC_NS",
	"GPIOC_S",
	"GPIOD_NS",
	"GPIOD_S",
	"GPIOE_NS",
	"GPIOE_S",
	"GPIOF_NS",
	"GPIOF_S",
	"GPIOG_NS",
	"GPIOG_S",
	"GPIOH_NS",
	"GPIOH_S",
	"GPIOI_NS",
	"GPIOI_S",
	"DMAC_E907_NS",
	"DMAC_E907_S",
	"DE",
	NULL,
	"G2D",
	"LCD",
	NULL,
	"DSI",
	[109 ... 110] = NULL,
	"CSI_DMA0",
	"CSI_DMA1",
	"CSI_DMA2",
	"CSI_DMA3",
	NULL,
	"CSI_PARSER0",
	"CSI_PARSER1",
	"CSI_PARSER2",
	NULL,
	"CSI_CMB",
	"CSI_TDM",
	"CSI_TOP_PKT",
	NULL,
	"CSI_ISP0",
	"CSI_ISP1",
	"CSI_ISP2",
	"CSI_ISP3",
	"VIPP0",
	"VIPP1",
	"VIPP2",
	"VIPP3",
	[132 ... 143] = NULL,
	"E907_MBOX_RX",     /* e907 msgbox read irq for e907 Interrupt */
	"E907_MBOX_TX",     /* e907 msgbox write irq for cpux Interrupt */
	"E907_WDG",
	"E907_TIMER0",
	"E907_TIMER1",
	"E907_TIMER2",
	"E907_TIMER3",
	NULL,
	"NMI",
	"PPU",
	"ALARM",
	"AHBS_HREADY_TIME_OUT",
	"PMC",
	"GIC_C0",
	"TWD",
	NULL,
};

struct arch_irq_desc
{
    hal_irq_handler_t handle_irq;
    void *data;
};

static struct arch_irq_desc arch_irqs_desc[E907_SUNXI_IRQ_MAX];

void show_irqs(void)
{
	int i;
	int enable;
	const char *status;
	const char *irq_name;
	printf("IRQ    Status    Name\r\n");
	for (i = 0; i < CLIC_IRQ_NUM; i++) {
		if (csi_vic_get_enabled_irq(i)) {
			irq_name = irq_string[i];
			if (irq_name)
				printf("%3d    Enabled   %s\r\n", i, irq_name);
			else
				printf("%3d    Enabled\r\n", i);
		}
	}
}

static hal_irqreturn_t clic_null_handler(void *data)
{
    return IRQ_NONE;
}

void enable_irq(unsigned int irq_num)
{
    if (NMI_EXPn != irq_num)
    {
        csi_vic_enable_irq(irq_num);
#ifdef CONFIG_STANDBY
        g_irq_status[irq_num] = 1;
#endif
    }
}

void disable_irq(unsigned int irq_num)
{
    if (NMI_EXPn != irq_num)
    {
        csi_vic_disable_irq(irq_num);
#ifdef CONFIG_STANDBY
        g_irq_status[irq_num] = 0;
#endif
    }
}

#ifdef CONFIG_STANDBY
void irq_suspend(void)
{
	int i = 0;

	for (i = 0; i < CLIC_IRQ_NUM; i++) {
		if (g_irq_status[i])
			csi_vic_disable_irq(i);
	}
}

void irq_resume(void)
{
	int i = 0;

	for (i = 0; i < CLIC_IRQ_NUM; i++) {
		if (g_irq_status[i])
			csi_vic_enable_irq(i);
	}
}
#endif

int32_t arch_request_irq(int32_t irq, hal_irq_handler_t handler, void *data)
{
    if (irq < CLIC_PERIPH_IRQ_OFFSET)
    {
        g_irqvector[irq] = (void *)handler;
        return irq;
    }

    if (irq + CLIC_PERIPH_IRQ_OFFSET < CLIC_IRQ_NUM)
    {
        if (handler && arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].handle_irq == (void *)clic_null_handler)
        {
            arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].handle_irq = (void *)handler;
            arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].data = data;
        }
        return irq;
    }
    printf("Wrong irq NO.(%"PRIu32") to request !!\n", irq);
    return -1;
}

void arch_free_irq(uint32_t irq)
{
    if (irq < CLIC_PERIPH_IRQ_OFFSET)
    {
        g_irqvector[irq] = (void *)Default_Handler;
        return;
    }
    if (irq + CLIC_PERIPH_IRQ_OFFSET < CLIC_IRQ_NUM)
    {
        arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].handle_irq = (void *)clic_null_handler;
    }
    return;
}

int request_threaded_irq(unsigned int irq, hal_irq_handler_t handler,
                         hal_irq_handler_t thread_fn, unsigned long irqflags,
                         const char *devname, void *dev_id)
{
    return arch_request_irq(irq, (void *)handler, dev_id);
}

const void *free_irq(int32_t irq)
{
    arch_free_irq(irq);
	return NULL;
}

unsigned long riscv_cpu_handle_interrupt(unsigned long scause, unsigned long sepc, unsigned long stval, irq_regs_t *regs)
{
    printf("E907 will not support the interrupt mode!\n");
	printf("cause:0x%08x mepc:0x%08x mtval:0x%08x\r\n");
	return 0;
}

void clic_common_handler(void)
{
    int id = (__get_MCAUSE() & 0xfff) - CLIC_PERIPH_IRQ_OFFSET;
    if (arch_irqs_desc[id].handle_irq &&
        arch_irqs_desc[id].handle_irq != (void *)clic_null_handler)
    {
        arch_irqs_desc[id].handle_irq(arch_irqs_desc[id].data);
    }
    else
    {
        printf("no handler for irq %d\n", id);
    }
}

void irq_vectors_init(void)
{
    int i;

    for (i = CLIC_PERIPH_IRQ_OFFSET; i < CLIC_IRQ_NUM; i++)
    {
#ifdef CONFIG_STANDBY
		g_irq_status[i] = 0;
#endif
        disable_irq(i - CLIC_PERIPH_IRQ_OFFSET);
        g_irqvector[i] = (void *)clic_common_handler;
        arch_irqs_desc[i - CLIC_PERIPH_IRQ_OFFSET].handle_irq = (void *)clic_null_handler;
        arch_irqs_desc[i - CLIC_PERIPH_IRQ_OFFSET].data = NULL;
    }

#ifdef CONFIG_STANDBY
	for (i = 0; i < CLIC_PERIPH_IRQ_OFFSET; i++) {
		if (csi_vic_get_enabled_irq(i))
			g_irq_status[i] = 1;
		else
			g_irq_status[i] = 0;
	}
#endif

    g_irqvector[CORET_IRQn] = SysTick_Handler;
}
