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
#include <stdio.h>
#include <string.h>
#include <init.h>
#include <arch.h>
#include <port.h>

#include <rtthread.h>

#ifdef CONFIG_DRIVERS_GPIO
#include <sunxi_drv_gpio.h>
#endif

#include <hal_cfg.h>

#define  STACK_CHECK_MAGICNUMBER    (0x5a5a5a5adeadbeef)
uint8_t melis_kernel_running = 0;

extern int  drv_clock_init(void);
extern int  finsh_system_init(void);
extern void timekeeping_init(void);
extern void trap_init(void);

#ifdef CONFIG_SYSCONF_BUILDIN
extern unsigned char blob_sysconfig_start[];
extern unsigned char blob_sysconfig_end[];
#endif

#ifdef CONFIG_CXX
int cplusplus_system_init(void);
#endif

int32_t awos_bsp_init(void)
{
#ifdef CONFIG_SYSCONF_BUILDIN
    Hal_Cfg_Init(blob_sysconfig_start, (int)blob_sysconfig_end - (int)blob_sysconfig_start + 1);
#endif

#ifdef CONFIG_DRIVER_CCMU
    drv_clock_init();
#endif

#ifdef CONFIG_DRIVER_GPIO
    drv_gpio_init();
#endif

#ifdef CONFIG_DRIVER_SERIAL
    void sunxi_driver_uart_init(void);
    sunxi_driver_uart_init();

    char uart_device[8] = {0};
    snprintf(uart_device, sizeof(uart_device), "uart%d", CONFIG_CLI_UART_PORT);
    rt_console_set_device(uart_device);
#endif

#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
    int multiple_console_init(void);
    multiple_console_init();
#endif

#ifdef CONFIG_E907_CPUFREQ
    int cpufreq_e907_init(void);
    cpufreq_e907_init();
#endif

#ifdef CONFIG_DRIVERS_MSGBOX
    uint32_t hal_msgbox_init(void);
    hal_msgbox_init();
#endif

    return 0;
}

asm(".option norvc");
static void awos_init_thread(void *para)
{
#ifdef CONFIG_PTHREAD
    int pthread_system_init(void);
    pthread_system_init();
#endif

#ifdef CONFIG_CXX
    cplusplus_system_init();
#endif

#ifdef CONFIG_STANDBY_MSGBOX
    int pm_standby_init(void);
    pm_standby_init();
#endif

    do_initcalls();

    finsh_system_init();

#ifdef CONFIG_MULTI_CONSOLE_RPMSG
    int rpmsg_multi_console_init_async(void);
    rpmsg_multi_console_init_async();
#endif
    app_entry(NULL);
}

asm(".option norvc");
void start_kernel(unsigned long spl_nanosec, unsigned long misa, unsigned long sbi_start, unsigned long sbi_end)
{
    awos_arch_lock_irq();
    trap_init();

#ifndef CONFIG_CPU_DCACHE_DISABLE
    extern void dcache_enable(void);
    dcache_enable();
#endif

    awos_init_bootstrap(awos_bsp_init, awos_init_thread, spl_nanosec);

    /* never come back */
    CODE_UNREACHABLE;
}
