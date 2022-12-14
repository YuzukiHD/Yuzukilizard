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
#include <stdio.h>
#include <stdlib.h>
#include <sunxi_drv_keyboard.h>
#include <sunxi_input.h>
#include <log.h>
#include <debug.h>
#include <init.h>
#include <kapi.h>
#include "sunxi_hal_tpadc.h"

#define TPCHANNEL0 0
#define TPCHANNEL1 1
#define TPCHANNEL2 2
#define TPCHANNEL3 3


typedef struct __DRV_TPADC_KEY_T
{
    rt_sem_t power_sem;
    __hdle pkeyTimer;
    unsigned int tpadc_data[4];
    unsigned int tpadc_flag[4];
    unsigned int tempdata_key;
    unsigned int whdata;
}__tpdac_data_t;

extern int32_t console_LKeyDevEvent(__input_dev_t *dev, uint32_t type, uint32_t code, int32_t value);

static __tpdac_data_t *keydata;
static hal_sem_t keytimer_sem;
static int init_flag = 0;;
static int key_number(__u32 data ,__u32 flag)
{
    __u32 key_code;
    static __u32 cnt;
    __wrn("key_number data = %d",data);
    if(data < 270)
    {
        key_code = KPAD_NUM1;
    }
    else if(data > 270 && data < 695)
    {
        key_code = KPAD_NUM2;
    }
    else if(data > 695 && data < 1130)
    {
        key_code = KPAD_NUM3;
    }
    else if(data > 1130 && data < 1572)
    {
        key_code = KPAD_NUM4;
    }
     else if(data > 1572 && data < 1987)
    {
        key_code = KPAD_NUM5;
    }
     else if(data > 1987 && data < 2337)
    {
        key_code = KPAD_NUM6;
    }
    else if(data > 2337 && data < 2652)
    {
        key_code = KPAD_NUM7;
    }
    else if(data > 2652 && data < 3062)
    {
        key_code = KPAD_NUM8;
    }
    else if(data > 3062 && data < 3497)
    {
        key_code = KPAD_NUM9;
    }
    else if (data > 3497 && data < 3900)
    {
        key_code = KPAD_NUM0;
    }
    else
    {
        __wrn("no key");
		return -1;
    }
    
    if(flag == 1)
    {
        cnt++;
        if(cnt > 30)
        {
            flag = LONGKEY;
        }
    }
    else if(flag == 0)
    {
        cnt = 0;
    }
	console_LKeyDevEvent(NULL,	EV_KEY, key_code,	flag);
	console_LKeyDevEvent(NULL,	EV_SYN,  0,  0);
    rt_thread_delay(2);
    return 0;
}

static int key_function(__u32 data ,__u32 flag)
{
//__log("data = %d   flag = %d",data,flag);
    __u32 key_code;
    static __u32 cnt;
    
    __wrn("key_function data = %d",data);
    if(data < 270)
	{
    	key_code = KPAD_MUSIC;
	}
	else if(data > 270 && data < 690)
	{
    	key_code = KPAD_VIDIO;	
	}
	else if(data > 690 && data < 1140)
	{
    	key_code = KPAD_FM;	
	}
	else if(data > 1140 && data < 1588)
	{
    	key_code = KPAD_RECORD;	
	}
	 else if(data > 1588 && data < 2008)
	{
		key_code = KPAD_MENU;	
	}
	 else if(data > 2008 && data < 2362)
	{
		key_code = KPAD_LCDOFF;	
	}
	else if(data > 2362 && data < 2674)
	{
	    key_code = KPAD_UP;	
	}
	else if(data > 2674 && data < 3081)
	{
    	key_code = KPAD_ENTER;	
	}
	else if(data > 3081 && data < 3520)
	{
    	key_code = KPAD_RETURN;	
	}
	else if (data > 3520 && data < 3900)
	{
    	key_code = KPAD_DOWN;	
	}
	else
	{
		__wrn("no key");
		return -1;
	}

    if(flag == 1)
    {
        cnt++;
        if(cnt > 30)
        {
            flag = LONGKEY;
        }
    }
    else if(flag == 0)
    {
        cnt = 0;
    }
	console_LKeyDevEvent(NULL,	EV_KEY, key_code,	flag);
	console_LKeyDevEvent(NULL,	EV_SYN,  0,  0);
    rt_thread_delay(2);

    return 0;
}

static int key_wheel(unsigned int data)
{
    unsigned int sent_data;
    sent_data = WHEEL_MASK(data);
    console_LKeyDevEvent(NULL,  EV_SW, sent_data,  1);
    console_LKeyDevEvent(NULL,  EV_SYN,  0,  0);
    rt_thread_delay(2);
    console_LKeyDevEvent(NULL,  EV_SW, sent_data,  0);
    console_LKeyDevEvent(NULL,  EV_SYN,  0,  0); 

    return 0;
}

static void keyTimercallback(void *parg)
{
    unsigned int i,j;
	hal_sem_wait(keytimer_sem);
    for(i = 2 ; i < 4; i++)
    {
        if(keydata->tpadc_data[i] < TPKEY_DOWN_VALUE)
        {

            __inf("channel %d key down  data = %d",i,keydata->tpadc_data[i]);
            keydata->tempdata_key = keydata->tpadc_data[i];

            if(i == 2)//number
            {
                key_number(keydata->tempdata_key,KEYBOARD_DOWN);
            }
            else if(i == 3)//function
            {
                key_function(keydata->tempdata_key,KEYBOARD_DOWN);
            }
            keydata->tpadc_flag[i] = 1;
        }
        else
        {
            if(keydata->tpadc_flag[i] == 1)
            {
                __inf("channel %d key up  data = %d",i,keydata->tempdata_key);
                if(i == 2)//number
                {
                    key_number(keydata->tempdata_key,KEYBOARD_UP);
                }
                else if(i == 3)//function
                {
                    key_function(keydata->tempdata_key,KEYBOARD_UP);
                }
            }
            keydata->tpadc_flag[i] = 0;
        }
        
    }
		if(abs(keydata->tpadc_data[1] - keydata->whdata) > SHAKE_VALUE)
		{
			unsigned int tempdata = 0;
			for(i=0;i<3;i++)
			{
				tempdata += keydata->tpadc_data[1];
				rt_thread_delay(1);
			}
			tempdata = tempdata / 3;
    		if(abs(keydata->tpadc_data[1] - keydata->whdata) > SHAKE_VALUE)
			{
				keydata->whdata = tempdata;
				__inf("wheel button sent data %d",keydata->whdata);                
                key_wheel(keydata->whdata);
			}
		}
		hal_sem_post(keytimer_sem);
}

int tpadc_key_callback(uint32_t data, int channel)
{
	switch (channel){
	case TP_CH0_SELECT:
		//__inf("data1 = %d",data);//
		keydata->tpadc_data[0] = data;
		break;
	case TP_CH1_SELECT:
		//__inf("data2 = %d",data);//旋钮
		keydata->tpadc_data[1] = data;
		break;
	case TP_CH2_SELECT:
		//__inf("data3 = %d",data);//功能
		keydata->tpadc_data[2] = data;
		break;
	case TP_CH3_SELECT:
		//__inf("data4 = %d",data);//数字
		keydata->tpadc_data[3] = data;
        break;
	default:
        SUNXIKBD_ERR("adc channel err\n");
   		return -1;
	}
    return 0;
}

rt_err_t tpkey_open(struct rt_device *dev, rt_uint16_t oflag)
{

	rt_size_t ret = 0;
    if (dev == NULL)
    {
		SUNXIKBD_ERR("dev == NULL\n");
        return -1;
    }
	if(init_flag > 0)
	{
		return 0;
	}
	init_flag = 1;
	keydata = rt_malloc(sizeof(__tpdac_data_t));
    if (keydata == NULL)
    {
		SUNXIKBD_ERR("tpadc keydata == NULL\n");
        return -1;
    }	
	rt_memset(keydata,0,sizeof(__tpdac_data_t));
	keytimer_sem = hal_sem_create(1);
	if(keytimer_sem == NULL)
	{
	   SUNXIKBD_ERR("keytimer_sem == NULL\n");
	   rt_free(keydata);
	   return -1;
	}

    hal_tpadc_adc_init();
	hal_tpadc_adc_channel_init(TPCHANNEL1);//ch1
	hal_tpadc_adc_channel_init(TPCHANNEL2);//ch2
	hal_tpadc_adc_channel_init(TPCHANNEL3);//ch3
	hal_tpadc_adc_register_callback(TPCHANNEL1, tpadc_key_callback);
	hal_tpadc_adc_register_callback(TPCHANNEL2, tpadc_key_callback);
	hal_tpadc_adc_register_callback(TPCHANNEL3, tpadc_key_callback);
    keydata->pkeyTimer = rt_timer_create("key_timer", keyTimercallback, keydata, 2, RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
	if(keydata->pkeyTimer == NULL)
	{
		SUNXIKBD_ERR("tpadc_key timer create err\n");
	}

    rt_timer_start(keydata->pkeyTimer);
    return 0;
}

static rt_err_t tpkey_close(struct rt_device *dev)
{
	rt_size_t ret = 0;
    if (dev == NULL)
    {
        while (1);
    }
    rt_timer_stop(keydata->pkeyTimer);
	hal_sem_wait(keytimer_sem);
	rt_timer_delete(keydata->pkeyTimer);
	hal_sem_post(keytimer_sem);
	hal_tpadc_adc_exit();

	hal_sem_delete(keytimer_sem);
	keytimer_sem = NULL;
	rt_free(keydata);
    keydata = NULL;
	init_flag = 0;
    return 0;
}

static rt_err_t tpkey_init(struct rt_device *dev)
{
    return 0;
}

static rt_size_t tpkey_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return 0;
}

static rt_size_t tpkey_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    return 0;
}

static rt_err_t tpkey_control(struct rt_device *dev, int cmd, void *args)
{
	if (dev == NULL)
    {
        SUNXIKBD_ERR("dev = NULL\n");
        while (1);
    }
	switch(cmd)
	{
		case TPADC_KEY_WHEEL_GET_VAL:
        {
        	uint32_t *vol = (uint32_t *)args;
			*vol = keydata->whdata;
            return 0;
        }
	}
    return 0;
}

static int init_tpadc_keyboard_device(struct rt_device *dev)
{
    rt_err_t ret = -1;
    
    if(!dev)
    {
        return ret;
    }
    dev->init        = tpkey_init;
    dev->open        = tpkey_open;
    dev->close       = tpkey_close;
    dev->read        = tpkey_read;
    dev->write       = tpkey_write;
    dev->control     = tpkey_control;

    ret = rt_device_register(dev, "tpadc_key", RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        SUNXIKBD_ERR("fatal error, ret %ld, register device to rtsystem failure.\n", ret);
        software_break();
        return ret;
    }

    return ret;
}

int init_tpadc_key(void)
{
    struct rt_device *device;

    device = rt_device_create(RT_Device_Class_Char, 0);
    if (!device)
    {
        __wrn("init_tpadc_key err");
        return -1;
    }
    init_tpadc_keyboard_device(device);
	return 0;
}
device_initcall(init_tpadc_key);

