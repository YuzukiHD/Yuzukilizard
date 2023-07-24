
/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.


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

#ifndef __SUNXI_HAL_PWM_H__
#define __SUNXI_HAL_PWM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <hal_clk.h>
#include <hal_reset.h>
#include <sunxi_hal_common.h>
// #include <init.h>
#include <pwm/platform_pwm.h>

//#define CONFIG_DRIVERS_PWM_DEBUG
#ifdef CONFIG_DRIVERS_PWM_DEBUG
#define PWM_INFO(fmt, arg...) hal_log_info(fmt, ##arg)
#else
#define PWM_INFO(fmt, arg...) do {}while(0)
#endif

#define PWM_ERR(fmt, arg...) hal_log_err(fmt, ##arg)

#define PRESCALE_MAX 256

/*************
 *SET_BITS set
 * **********/
#define SETMASK(width, shift)   ((width?((-1U) >> (32-width)):0)  << (shift))
#define CLRMASK(width, shift)   (~(SETMASK(width, shift)))
#define GET_BITS(shift, width, reg)     \
	(((reg) & SETMASK(width, shift)) >> (shift))
#define SET_BITS(shift, width, reg, val) \
	(((reg) & CLRMASK(width, shift)) | (val << (shift)))

/* define shift and width */
#define PWM_CLK_SRC_SHIFT 0x7
#define PWM_CLK_SRC_WIDTH 0x2

#define PWM_DIV_M_SHIFT 0x0
#define PWM_DIV_M_WIDTH 0x4

#define PWM_PRESCAL_SHIFT 0x0
#define PWM_PRESCAL_WIDTH 0x8

#define PWM_ACT_CYCLES_SHIFT 0x0
#define PWM_ACT_CYCLES_WIDTH 0x10

#define PWM_PERIOD_CYCLES_SHIFT 0x10
#define PWM_PERIOD_CYCLES_WIDTH 0x10

#define PWM_DZ_EN_SHIFT  0x0
#define PWM_DZ_EN_WIDTH  0x1
#define PWM_PDZINTV_SHIFT  0x8
#define PWM_PDZINTV_WIDTH  0x8

#define PWM_BIND_NUM 2

/*****************************************************************************
 * Enums
 *****************************************************************************/
typedef unsigned long pwm_status_t;

typedef enum
{
    PWM_CLK_OSC,
    PWM_CLK_APB,
} hal_pwm_clk_src;

typedef enum
{
    PWM_POLARITY_INVERSED = 0,
    PWM_POLARITY_NORMAL = 1,
} hal_pwm_polarity;

typedef enum
{
    PWM_CONTROL = 0,
    PWM_CHANNEL_INT = 1,
    PWM_CHANNEL_UNINT = 2,
} hal_pwm_cmd_t;

typedef struct pwm_config
{
    uint32_t        duty_ns;
    uint32_t        period_ns;
    hal_pwm_polarity    polarity;
} pwm_config_t;

static u32 hal_pwm_regs_offset[] = {
    PWM_PIER,
    PWM_CIER,
    PWM_PCCR01,
    PWM_PCCR23,
    PWM_PCCR45,
    PWM_PCCR67,
    PWM_PCGR,
    PWM_PDZCR01,
    PWM_PDZCR23,
    PWM_PDZCR45,
    PWM_PDZCR67,
    PWM_PER,
    PWM_CER,
    PWM_PCR,
    PWM_PPR,
    PWM_CCR,
    PWM_PCNTR,
};

typedef struct
{
    hal_clk_type_t pwm_clk_type;
    hal_clk_id_t pwm_bus_clk_id;
    hal_clk_t pwm_bus_clk;
    hal_reset_type_t pwm_reset_type;
    hal_reset_id_t pwm_reset_id;
    struct reset_control *pwm_reset;

    bool pin_state[PWM_NUM];
    gpio_pin_t pin[PWM_NUM];
    gpio_muxsel_t enable_muxsel[PWM_NUM];
    u32 regs_backup[ARRAY_SIZE(hal_pwm_regs_offset)];
} hal_pwm_t;

pwm_status_t hal_pwm_init(void);
pwm_status_t hal_pwm_control(int channel, struct pwm_config *config_pwm);
pwm_status_t hal_pwm_release(int channel);
void hal_pwm_enable_controller(uint32_t channel_in);
void hal_pwm_disable_controller(uint32_t channel_in);
pwm_status_t hal_pwm_deinit(void);

int hal_pwm_resume(void *dev);
int hal_pwm_suspend(void *dev);

#ifdef __cplusplus
}
#endif

#endif /* __SUNXI_HAL_PWM_H__ */


