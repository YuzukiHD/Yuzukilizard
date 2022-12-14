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
#include "soc.h"
#include <core_rv32.h>
#include <hal_interrupt.h>

#define CONFIG_CPU_E907FD 1

static int g_system_clock = IHS_VALUE;

extern void irq_vectors_init(void);
extern int __Vectors;
extern void dcache_enable(void);

int32_t drv_get_sys_freq(void)
{
    return g_system_clock;
}

static void _system_init_for_kernel(void)
{
    irq_vectors_init();

    csi_coret_config(drv_get_sys_freq() / CONFIG_HZ, CORET_IRQn);    //10ms
    hal_enable_irq(CORET_IRQn);
}

void SystemInit(void)
{
    int i;
    uint32_t mstatus = 0;

    /* enable mstatus FS */
#ifdef CONFIG_ARCH_RISCV_FPU
    mstatus = __get_MSTATUS();
    mstatus |= (1 << 13);
    __set_MSTATUS(mstatus);
#endif

    /* enable mxstatus THEADISAEE */
    uint32_t mxstatus = __get_MXSTATUS();
    mxstatus |= (1 << 22);
    /* enable mxstatus MM */
#ifdef CONFIG_ARCH_RISCV_FPU
    mxstatus |= (1 << 15);
#endif
    __set_MXSTATUS(mxstatus);

    /* get interrupt level from info */
    CLIC->CLICCFG = (((CLIC->CLICINFO & CLIC_INFO_CLICINTCTLBITS_Msk) >> CLIC_INFO_CLICINTCTLBITS_Pos) << CLIC_CLICCFG_NLBIT_Pos);

    for (i = 0; i < sizeof(CLIC->CLICINT)/sizeof(CLIC->CLICINT[0]); i++)
    {
        CLIC->CLICINT[i].IP = 0;
        CLIC->CLICINT[i].ATTR = 1; /* use vector interrupt */
    }

    /* tspend use positive interrupt */
    CLIC->CLICINT[Machine_Software_IRQn].ATTR = 0x3;

    hal_enable_irq(Machine_Software_IRQn);

    _system_init_for_kernel();
}

/* enable mexstatus SPUSHEN and SPSWAPEN */
#ifdef CONFIG_ARCH_RISCV_FPU
void context_mexstatus(void)
{

    unsigned int  mexstatus;
    mexstatus  = __get_MEXSTATUS();
    mexstatus |= (0x2 << 16);
    __set_MEXSTATUS(mexstatus);
}
#endif

#ifdef CONFIG_STANDBY
void clic_suspend(void)
{
	return;
}

void clic_resume(void)
{
	int i;
    uint32_t mstatus = 0;

    /* enable mstatus FS */
#ifdef CONFIG_ARCH_RISCV_FPU
    mstatus = __get_MSTATUS();
    mstatus |= (1 << 13);
    __set_MSTATUS(mstatus);
#endif

    /* enable mxstatus THEADISAEE */
    uint32_t mxstatus = __get_MXSTATUS();
    mxstatus |= (1 << 22);
    /* enable mxstatus MM */
#ifdef CONFIG_ARCH_RISCV_FPU
    mxstatus |= (1 << 15);
#endif
    __set_MXSTATUS(mxstatus);

    /* get interrupt level from info */
    CLIC->CLICCFG = (((CLIC->CLICINFO & CLIC_INFO_CLICINTCTLBITS_Msk) >> CLIC_INFO_CLICINTCTLBITS_Pos) << CLIC_CLICCFG_NLBIT_Pos);

    for (i = 0; i < sizeof(CLIC->CLICINT)/sizeof(CLIC->CLICINT[0]); i++)
    {
        CLIC->CLICINT[i].IP = 0;
        CLIC->CLICINT[i].ATTR = 1; /* use vector interrupt */
    }

    /* tspend use positive interrupt */
    CLIC->CLICINT[Machine_Software_IRQn].ATTR = 0x3;

    csi_coret_config(drv_get_sys_freq() / CONFIG_HZ, CORET_IRQn);
    hal_enable_irq(CORET_IRQn);

    dcache_enable();

}
#endif
