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
#include <init.h>
#include <kapi.h>

extern int32_t console_LKeyDevEvent(__input_dev_t *dev, uint32_t type, uint32_t code, int32_t value);


static uint32_t key_flag = 0;
static uint8_t filter_cnt = 3;
////////////////
static int keythread_id;
static rt_sem_t key_sem;
static uint8_t key_state = GPADC_UP;
static unsigned int vol_data;
///////////////
#if defined(CONFIG_SUNXI_QA_TEST)
struct sunxikbd_config key_config =
{
    .measure = 1800,
    .key_num = 5,
    .key_vol = {210, 410, 590, 750, 880},
    .scankeycodes = {KPAD_UP, KPAD_DOWN, KPAD_ENTER, KEY_RESERVED, KPAD_RETURN},
    .name = "sunxi-keyboard"
};
#else
struct sunxikbd_config key_config =
{
    .measure = 1800,
    .key_num = 5,
    .key_vol = {210, 410, 590, 750, 880},
    .scankeycodes = {KPAD_UP, KPAD_DOWN, KPAD_ENTER, KPAD_MENU, KPAD_RETURN},
    .name = "sunxi-keyboard"
};

#endif
static unsigned char keypad_mapindex[128] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0,      /* key 1, 0-8 */
    1, 1, 1, 1, 1,                  /* key 2, 9-13 */
    2, 2, 2, 2, 2, 2,               /* key 3, 14-19 */
    3, 3, 3, 3, 3, 3,               /* key 4, 20-25 */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    /* key 5, 26-36 */
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,    /* key 6, 37-39 */
    6, 6, 6, 6, 6, 6, 6, 6, 6,      /* key 7, 40-49 */
    7, 7, 7, 7, 7, 7, 7             /* key 8, 50-63 */
};

struct sunxikbd_drv_data *key_data  = NULL;
struct sunxi_input_dev *sunxikbd_dev = NULL;

static void key_enable_irq(hal_gpadc_channel_t channal)
{
    uint32_t reg_val;
    reg_val = readl((unsigned long)(0x02009000) + 0x20);
    reg_val |= (1 << channal);
    writel(reg_val, (unsigned long)(0x02009000) + 0x20);

}

static void key_disable_irq(hal_gpadc_channel_t channal)
{
    uint32_t reg_val;
    reg_val = readl((unsigned long)(0x02009000) + 0x20);
    reg_val &= ~(1 << channal);
    writel(reg_val, (unsigned long)(0x02009000) + 0x20);

}


int keyboard_irq_callback(uint32_t data_type, uint32_t data)
{
    key_disable_irq(0);
        data = ((VOL_RANGE / 4096) * data); /* 12bits sample rate */
    vol_data = data / 1000;

    if(vol_data > SUNXIKEY_DOWN)
    {
        key_enable_irq(0);
        return 0;
    }
    key_state = data_type;
    esKRNL_SemPost(key_sem);
    return 0;
}

static u32 key_data_key_xfer(u32 data)
{

    key_data->compare_before = (data / SCALE_UNIT)&0xff;

    if (key_data->compare_before >= key_data->compare_later - 1
        && key_data->compare_before <= key_data->compare_later + 1)
        key_data->key_cnt++;
    else
        key_data->key_cnt = 0;

    key_data->compare_later = key_data->compare_before;
    if (key_data->key_cnt >= filter_cnt)
    {
        key_data->compare_later = key_data->compare_before;
        key_data->key_code = keypad_mapindex[key_data->compare_before];
        key_data->compare_later = 0;
        key_data->key_cnt = 0;
        if (key_data->key_code != key_config.key_num)
        {
            return key_data->scankeycodes[key_data->key_code];
        }
    }
    return 0;
}

static int sunxikbd_data_init(struct sunxikbd_drv_data *key_data, struct sunxikbd_config *sunxikbd_config)
{
    int i, j = 0;
    int key_num = 0;
    unsigned int resol;
    unsigned int key_vol[KEY_MAX_CNT];

    key_num = sunxikbd_config->key_num;
    if (key_num < 1 || key_num > KEY_MAX_CNT)
    {
        return -1;
    }

    resol = sunxikbd_config->measure / MAXIMUM_SCALE;

    for (i = 0; i < key_num; i++)
    {
        key_data->scankeycodes[i] = sunxikbd_config->scankeycodes[i];
    }

    for (i = 0; i < key_num; i++)
    {
        key_vol[i] = sunxikbd_config->key_vol[i];
    }
    //key_vol[key_num] = sunxikbd_config->measure;

    for (i = 0; i < (key_num - 1); i++)
    {
        key_vol[i] += (key_vol[i + 1] - key_vol[i]) / 2;
    }

    for (i = 0; i < MAXIMUM_SCALE; i++)
    {
        if (i * resol > key_vol[j])
        {
            j++;
        }
        keypad_mapindex[i] = j;
    }

    key_data->last_key_code = INITIAL_VALUE;

    return 0;
}

static void sent_key_thread(void *parg)
{
    uint32_t press_cnt = 0;
    uint32_t scankeycodes = 0;
    uint32_t key_flag = 0;//if long press

    while(1)
    {
        esKRNL_SemPend(key_sem, 0, NULL);
        if(key_state == GPADC_DOWN)
        {
            scankeycodes = key_data_key_xfer(vol_data);
            if(scankeycodes == 0)
            {
                key_flag = 0;
                key_enable_irq(0);
                continue;
            }
            press_cnt++;
            if(press_cnt > 30)
            {
                key_flag = 2;
            }
            else
            {
                key_flag = 1;
            }
            console_LKeyDevEvent(NULL,    EV_KEY,   scankeycodes,    key_flag);
            console_LKeyDevEvent(NULL,    EV_SYN,   0,   0);
        }
        else if(key_state == GPADC_UP)
        {
            __log("key up");
            key_flag = 0;
            press_cnt = 0;
            console_LKeyDevEvent(NULL,    EV_KEY,   scankeycodes, 0);
            console_LKeyDevEvent(NULL,    EV_SYN,   0,    0);
        }
        else
        {
            __err("key err");
        }
        esKRNL_TimeDly(2);
        key_enable_irq(0);
    }
}

int sunxi_keyboard_init(void)
{
    int i;
    //__log("sunxi keyboard init.");
    key_sem = esKRNL_SemCreate(1);
    keythread_id = esKRNL_TCreate(sent_key_thread, NULL, 0x1000, KRNL_priolevel1);
    if(keythread_id == 0)
	{
		__err("gpadc_key thread create err\n");
	}

    key_data = (struct sunxikbd_drv_data *)malloc(sizeof(struct sunxikbd_drv_data));

    if (key_data == NULL)
    {
        SUNXIKBD_ERR("alloc key data memory failed.\n");
        return -1;
    }

    memset(key_data, 0x00, sizeof(struct sunxikbd_drv_data));
    if (sunxikbd_data_init(key_data, &key_config) < 0)
    {
        SUNXIKBD_ERR("init sunxikbd data failed.\n");
        return -1;
    }

    sunxikbd_dev = sunxi_input_allocate_device();
    if (sunxikbd_dev == NULL)
    {
        SUNXIKBD_ERR("init sunxikbd dev failed.\n");
        return -1;
    }

    sunxikbd_dev->name = key_config.name;
    for (i = 0; i < key_config.key_num; i++)
    {
        input_set_capability(sunxikbd_dev, EV_KEY, key_data->scankeycodes[i]);
    }

    sunxi_input_register_device(sunxikbd_dev);

    hal_gpadc_init();
    hal_gpadc_channel_init(GP_CH_0);
    hal_gpadc_register_callback(GP_CH_0, keyboard_irq_callback);
    return 0;
}
late_initcall(sunxi_keyboard_init);
