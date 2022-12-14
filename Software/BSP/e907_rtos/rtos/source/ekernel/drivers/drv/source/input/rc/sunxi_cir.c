/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
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

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sunxi_drv_cir.h>
#include "sunxi_hal_cir.h"
#include "rc-core-prive.h"
#include <sunxi_input.h>
#include <init.h>
#include <standby/standby.h>

#if (CONFIG_LOG_DEFAULT_LEVEL == 0)
#define printf(...)
#endif

#define CIR_UNIT_24M_DIV256		(10700)

static hal_cir_info_t cir_info[CIR_MASTER_NUM];
#if 1
#include <kapi.h>
#include "ir_keymap.h"
extern int32_t console_LKeyDevEvent(__input_dev_t *dev, uint32_t type, uint32_t code, int32_t value);

__u32 *ir_solution_keymapping = NULL;
/*
****************************************************************************************************
*
*  FunctionName:           lradckey_keydown
*
*  Description:
*              Press the key and call ioctrl to send the key press message. Callback function.
*
*  Parameters:
*       keyvalue :
*
*  Return value:
*  
*  Notes:
*
****************************************************************************************************
*/
static  void irkey_keydown(__u32 keyvalue)
{
	__inf("irkey_keydown keyvalue= %x",ir_solution_keymapping[keyvalue]);
	console_LKeyDevEvent(NULL,	EV_KEY, ir_solution_keymapping[keyvalue],	1);
	console_LKeyDevEvent(NULL,	EV_SYN,  0,  0);

    return;
}

/*
****************************************************************************************************
*
*  FunctionName:           lradckey_keyup
*
*  Description:
*              Press the key up and call ioctrl to send the key up message. Callback function.
*
*  Parameters:
*       keyvalue :
*
*  Return value:
*  
*  Notes:
*
****************************************************************************************************
*/
static  void irkey_keyup(__u32 keyvalue)
{
	hal_cir_info_t *ir_key = NULL;
	ir_key = &cir_info[0];

	ir_key->press_cnt = 0;
	__inf("irkey_keyup =%x",ir_solution_keymapping[keyvalue]);
	console_LKeyDevEvent(NULL,	EV_KEY, ir_solution_keymapping[keyvalue],	0);
	console_LKeyDevEvent(NULL,	EV_SYN,  0,  0);
	return;
}
static  void  aw_ir_key_keyup(hal_cir_info_t *ir_key, __u32 key_value)
{
	ir_key->crt_value = 0xff;
	ir_key->pre_value = 0xff;

	if (ir_key->key_up) {
 		ir_key->key_up(key_value);
	}

    return;
}

static void aw_ir_key_timer_do(void *pArg)
{
	hal_cir_info_t *ir_key = NULL;
	ir_key = &cir_info[0];
	aw_ir_key_keyup(ir_key, ir_key->crt_value);
	__inf("aw_ir_key_keyup");
	//aw_ir_key_keyup(ir_key, ir_key->crt_value);
	//ir_key->keyup_cnt  = 0;
	//ir_key->repeat_key = IR_REPEAT_KEY_OFF;//repeat code flag
	//IR_DBG("key up, key = %x ", ir_key->crt_value);
	return;
}

#endif

cir_status_t hal_cir_port_is_valid(cir_port_t port)
{
	return port < CIR_MASTER_NUM;
}

static void  hal_cir_port_config(cir_port_t port)
{
	/*enable cir mode*/
	sunxi_cir_mode_enable(port, 1);

	/*config cir mode*/
	sunxi_cir_mode_config(port, CIR_BOTH_PULSE);

	/*config cir sample clock to IR/CLK256*/
	sunxi_cir_sample_clock_select(port, CIR_CLK_DIV256);

	/*config cir noise threshold for cir*/
	sunxi_cir_sample_noise_threshold(port, CIR_NOISE_THR_NEC);

	/*config cir idle threshold for cir*/
	sunxi_cir_sample_idle_threshold(port, 11);

	/*config cir active threshold for cir*/
	sunxi_cir_sample_active_threshold(port, 32);

	/*control cir activr threshold for cir*/
	sunxi_cir_sample_active_thrctrl(port, 0);

	/*config cir receive byte level*/
	sunxi_cir_fifo_level(port, 20);

	/*enable cir irq*/
	sunxi_cir_irq_enable(port, ROI_EN | PREI_EN | RAI_EN);

	/*invert cir signal*/
	sunxi_cir_signal_invert(port, 1);

	/*enable cir global module*/
	sunxi_cir_module_enable(port, 1);
}

static void hal_cir_receive_data(cir_port_t port, uint32_t data)
{
	hal_cir_info_t *info = &cir_info[port];

	bool pulse = 0;
	uint32_t duration = 0;

	pulse = data >> 7;
	duration = data & 0x7f;

	if (info->is_receiving && pulse != info->pulse) {
		if (info->index < CIR_FIFO_SIZE)
			info->index++;
		else
			return;
	}

	info->event[info->index].duration += duration * CIR_UNIT_24M_DIV256;
	info->event[info->index].pulse = pulse;
	info->is_receiving = true;
	info->pulse = pulse;

}

static void hal_cir_clear_data(cir_port_t port)
{
	hal_cir_info_t *info = &cir_info[port];
	int i;

	for (i = 0; i < CIR_FIFO_SIZE; i++)
	{
		info->event[i].pulse = 0;
		info->event[i].duration = 0;
	}

	info->index = 0;
	info->pulse = false;
	info->is_receiving = false;

	nec_desc_init(&info->nec);
}

static void hal_cir_decode(cir_port_t port)
{
	int i;
	hal_cir_info_t *info = &cir_info[port];
	for(i = 0; i <= info->index; i++)
	{
		ir_nec_decode(info, info->event[i]);
	}
	hal_cir_clear_data(port);
}

static int hal_cir_handle_data(cir_port_t port, uint32_t data_type, uint32_t data)
{
	switch (data_type)
	{
		case RA:
			hal_cir_receive_data(port, data);
			break;
		case ROI:
			hal_cir_clear_data(port);
			break;
		case RPE:
			hal_cir_decode(port);
			break;
		default:
			break;
	}

	return 0;
}

void hal_cir_put_value(hal_cir_info_t *info, uint32_t scancode)
{
	input_set_capability(info->sunxi_ir_dev, EV_KEY, scancode);

	input_report_key(info->sunxi_ir_dev, scancode, 1);

	input_sync(info->sunxi_ir_dev);
}

void hal_cir_long_press_value(hal_cir_info_t *info, uint32_t scancode)
{
	__inf("long_key =%x",ir_solution_keymapping[scancode]);
	console_LKeyDevEvent(NULL,	EV_KEY, ir_solution_keymapping[scancode],	2);
	console_LKeyDevEvent(NULL,	EV_SYN,  0,  0);
	return;
}

#if 0
uint32_t hal_cir_get_value(cir_port_t port)
{
	hal_cir_info_t *info;
	uint32_t value;

	if (!hal_cir_port_is_valid(port))
		return 0;

	info = &cir_info[port];

	value = info->value;

	hal_cir_put_value(info, 0);

	return value;
}
#endif

#ifdef CONFIG_STANDBY
int hal_cir_suspend(cir_port_t port)
{
	sunxi_cir_suspend(port);

	return 0;
}

int hal_cir_resume(cir_port_t port)
{
	sunxi_cir_resume(port);

	return 0;
}
#endif

cir_status_t hal_cir_master_deinit(cir_port_t port)
{
	if (!hal_cir_port_is_valid(port))
		return CIR_PORT_ERR;

	sunxi_cir_deinit(port);

	return 0;
}

int cir_master_init(void)
{
	int ret;
	cir_port_t port;

	for (port = 0; port < CIR_MASTER_NUM; port++) {
		hal_cir_info_t *info = &cir_info[port];
		if (!hal_cir_port_is_valid(port)) {
			printf("valid failed\n");
			return CIR_PORT_ERR;
		}
		ret = sunxi_cir_init(port);
		if (ret)
			return ret;

		info->sunxi_ir_dev = sunxi_input_allocate_device();
		if (!info->sunxi_ir_dev) {
			printf("init sunxi_ir_dev failed\n");
		}

		info->sunxi_ir_dev->name = "sunxi-ir-rx";

		hal_cir_clear_data(port);
		hal_cir_port_config(port);

		sunxi_input_register_device(info->sunxi_ir_dev);
		sunxi_cir_callback_register(port, hal_cir_handle_data);
		info->key_down	= irkey_keydown;
		info->key_up	= irkey_keyup;
		info->hTimer = rt_timer_create("ir_timer", aw_ir_key_timer_do, NULL, 13, RT_TIMER_FLAG_ONE_SHOT);
		info->crt_value = 0xff;
		info->pre_value = 0xff;
		info->press_cnt = 0;
		info->user_code = NEC_IR_ADDR_CODE;/*if user_code is 0, the user code is not checked*/
		info->valid = true;

		if(info->user_code == NEC_IR_ADDR_CODE) {
			ir_keymap[18]  = KPAD_MENU;
			ir_keymap[64] = KPAD_RETURN;/* learning */
			ir_keymap[170] = KPAD_UP;
			ir_keymap[160] = KPAD_RIGHT;
			ir_keymap[96] = KPAD_ENTER;
			ir_keymap[122] = KPAD_LEFT;
			ir_keymap[224] = KPAD_DOWN;
		}
		ir_solution_keymapping = ir_keymap;

	}
#ifdef CONFIG_STANDBY
	register_pm_dev_notify(hal_cir_suspend, hal_cir_resume, NULL);
#endif
	printf("the cir_master_init success\n");

	return ret;
}

late_initcall(cir_master_init);
