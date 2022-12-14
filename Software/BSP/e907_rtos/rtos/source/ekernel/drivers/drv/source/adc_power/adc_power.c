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
#include <hal_poll.h>
#include <aw_list.h>
#include <rtthread.h>
#include <typedef.h>
#include <debug.h>
#include <log.h>
#include <init.h>
#include <mod_power.h>
#include <hal_gpio.h>
#include <sunxi_hal_gpadc.h>
#include <kapi.h>


#define POWER_ADC_SPMPLE_NUM 3//6次AD采样取平均

#define NO_PMU_BATTERY_LEVEL0   0
#define NO_PMU_BATTERY_LEVEL1   20
#define NO_PMU_BATTERY_LEVEL2   40
#define NO_PMU_BATTERY_LEVEL3   60
#define NO_PMU_BATTERY_LEVEL4   80
#define NO_PMU_BATTERY_LEVEL5   100
#define GPADC_DATA_REG 0x02009080

typedef struct __DRV_POWER_CTL_T
{
    rt_sem_t power_sem;
    __hdle pTimer;
    rt_device_t adc_fd;
    __hdle det_io_hdl;

} __drv_power_ctl_t;

static __drv_power_ctl_t *power_ctl = NULL;
static __drv_power_fuelguage_t *fuelguage = NULL;
static  bool init_flag;

static void adc_power_lock(void)
{
    int hal_sem_ret;
    hal_sem_ret = rt_sem_take(power_ctl->power_sem, 200);
    if (hal_sem_ret != 0)
    {
        __inf("power hal_sem_timedwait timeout\n");
    }
}

static void adc_power_unlock(void)
{
    int hal_sem_ret;
    hal_sem_ret = rt_sem_release(power_ctl->power_sem);
    if (hal_sem_ret != 0)
    {
        __inf("power hal_sem_post timeout\n");
    }
}

static __u32 CalcVoltage(__u32 ADCVal)
{
    // Vin = ADCVal *Vref(1.8V) /4095
    //Calculate the actual voltage of the battery
    __u32 VadcIO, Vbat;
    VadcIO = (ADCVal * 1000 * 1.8) / 4095;
    Vbat = VadcIO * 5 / 2;
    __inf("Vdata = %dmV   Vbat = %dmv", VadcIO, Vbat);
    return Vbat;
}
static void power_adc_init(void)
{
    hal_gpadc_init();
    hal_gpadc_channel_init(GP_CH_0);
    //hal_gpadc_register_callback(GP_CH_0, power_adc_irq_callback);
}
static void power_adc_uninit(void)
{
    hal_gpadc_channel_exit(GP_CH_0);
    hal_gpadc_deinit();
}
static __s32 dac_get_cur_value(void)
{
    __inf("adc data = %d", readl(GPADC_DATA_REG));
    return readl(GPADC_DATA_REG);
}

static __s32 power_get_bat_value(void)
{
    __u32 adc_value = 0;
    __u32   Voltage = 0;
    __u32 adc_average_value = 0;
    __u32 i;

    //get adc average value in 6 times
    for (i = 0; i < POWER_ADC_SPMPLE_NUM; i++)
    {
        adc_value += dac_get_cur_value();
        rt_thread_delay(1);
    }
    adc_average_value = adc_value / POWER_ADC_SPMPLE_NUM;

    //adc_value_to_bat_level
    Voltage =  CalcVoltage(adc_average_value);
    /*The battery voltage varies during charging*/
    adc_power_lock();
    if (fuelguage->charge_status == 1) //It's charging
    {
        __u32 reg_value = 0;

        fuelguage->is_full_charge = EPDK_FALSE;

        if (Voltage <= 3494)
        {
            //低电关机电压
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL0;
            //发送低电关机系统消息
            //__msg("low power voltage detect...");
            //esKSRV_SendMsg(KMSG_USR_KEY_POWEROFF, KMSG_PRIO_HIGH);
        }
        else if ((Voltage > 3494) && (Voltage <= 3636))
        {
            //电池电量为20%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL1;
        }
        else if ((Voltage > 3636) && (Voltage <= 3781))
        {
            //电池电量为40%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL2;
        }
        else if ((Voltage > 3781) && (Voltage <= 3924))
        {
            //电池电量为60%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL3;
        }
        else if ((Voltage > 3924) && (Voltage <= 4044))
        {
            //电池电量为80%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL4;
        }
        else if (Voltage > 4044)
        {
            //电池电量为100%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL5;
            __inf("fuelguage->is_full_charge");
            fuelguage->is_full_charge = EPDK_TRUE;
        }

    }
    else//It's not charging
    {
        if (Voltage <= 3450)
        {
            //低电关机电压
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL0;
            //发送低电关机系统消息
            //__msg("low power voltage detect...");
            //esKSRV_SendMsg(KMSG_USR_KEY_POWEROFF, KMSG_PRIO_HIGH);
        }
        else if ((Voltage > 3450) && (Voltage <= 3536))
        {
            //电池电量为20%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL1;
        }
        else if ((Voltage > 3536) && (Voltage <= 3681))
        {
            //电池电量为40%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL2;
        }
        else if ((Voltage > 3681) && (Voltage <= 3824))
        {
            //电池电量为60%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL3;
        }
        else if ((Voltage > 3824) && (Voltage <= 3919))
        {
            //电池电量为80%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL4;
        }
        else if (Voltage > 3919)
        {
            //电池电量为100%
            fuelguage->rest_vol = NO_PMU_BATTERY_LEVEL5;
        }
        fuelguage->is_full_charge = EPDK_FALSE;

    }
    fuelguage->bat_power = Voltage;
    adc_power_unlock();
    __wrn("adc_average_value = 0x%x  Voltage = %dmV  power_level = %d",adc_average_value,Voltage,fuelguage->rest_vol);
    return fuelguage->rest_vol;
}

static void power_check_dcin(void)
{
    gpio_data_t data;
    data = (gpio_data_t)esPINS_ReadPinData(power_ctl->det_io_hdl, NULL);

    adc_power_lock();
    if (data == GPIO_DATA_HIGH)
    {
        fuelguage->charge_status = 1;
    }
    else if (data == GPIO_DATA_LOW)
    {
        fuelguage->charge_status = 0;
    }
    else
    {
        fuelguage->charge_status = 0;
        __inf("get the IO status err");
    }
    adc_power_unlock();

    __inf("timer tick tock  fuelguage->charge_status = %d", fuelguage->charge_status);
}

static void PowerTimer(void *parg)
{
    power_check_dcin();
#ifdef CONFIG_PLAN_SPRITE
    power_get_bat_value();
#endif
}

static void power_det_io_init(void)
{
    int32_t ret;
    user_gpio_set_t gpio_set;

    rt_memset(&gpio_set, 0x00, sizeof(gpio_set));
    ret = esCFG_GetKeyValue("adc_power", "power_det_io", (__s32 *)&gpio_set, sizeof(user_gpio_set_t) / 4);
    if(ret < 0)
    {
        __err("get power_det_io fail");
        return;
    }
    if(power_ctl)
    {
        power_ctl->det_io_hdl = esPINS_PinGrpReq(&gpio_set, 1);
        if(!power_ctl->det_io_hdl)
        {
            __err("request power_det_io fail");
            return;
        }
    }
}

static rt_err_t power_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_size_t ret = 0;
    if(init_flag)
    {
        __wrn("adc power init already");
        return EPDK_OK;
    }

    init_flag = 1;

    if (dev == NULL)
    {
        __err("dev == NULL");
        goto err;
    }
    power_ctl = rt_malloc(sizeof(__drv_power_ctl_t));

    fuelguage = rt_malloc(sizeof(__drv_power_fuelguage_t));

    if (fuelguage == NULL || power_ctl == NULL)
    {
        __err("power fuelguage or power_ctl == NULL");
        goto err;
    }

    rt_memset(power_ctl, 0, sizeof(__drv_power_ctl_t));
    rt_memset(fuelguage, 0, sizeof(__drv_power_fuelguage_t));
    
    power_det_io_init();
    fuelguage->rest_vol = -1;
    fuelguage->bat_power = -1;
    fuelguage->charge_status = -1;
#ifdef CONFIG_PLAN_SPRITE
    power_adc_init();
#endif

    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        software_break();
        return ret;
    }

    power_ctl->power_sem = rt_sem_create("power_dev", 1, RT_IPC_FLAG_FIFO);

    power_ctl->pTimer = rt_timer_create("power_timer", PowerTimer, fuelguage, 100, RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
    PowerTimer(fuelguage);//run the timer once immediately
    if (power_ctl->pTimer == NULL)
    {
        __err("power pTimer == NULL");
        goto err;
    }
    __wrn("power_timer start");
    rt_timer_start(power_ctl->pTimer);

    return ret;

err:
    __err("serious err when openning power dev");
    return ret;
}

static rt_err_t power_close(struct rt_device *dev)
{
    rt_size_t ret = 0;
    if(init_flag == 1)
    {
        return 0;
    }
    __err("adc power close");

	if (dev == NULL)
    {
        while (1);
    }
    if(!power_ctl || !fuelguage)
    {
        return -1;
    }
    init_flag = 0;
#ifdef CONFIG_PLAN_SPRITE
    power_adc_uninit();
#endif
    rt_timer_stop(power_ctl->pTimer);
    rt_sem_delete(power_ctl->power_sem);
    power_ctl->power_sem = NULL;
    rt_timer_delete(power_ctl->pTimer);
    power_ctl->pTimer = NULL;
    if(!power_ctl->det_io_hdl)
    {
        esPINS_PinGrpRel(power_ctl->det_io_hdl, 0);
    }
    power_ctl->det_io_hdl = NULL;
    rt_free(fuelguage);
    rt_free(power_ctl);
    fuelguage = NULL;
    power_ctl = NULL;
    return ret;
}


static rt_err_t power_init(struct rt_device *dev)
{
    return 0;
}

static rt_size_t power_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    return 0;
}

static rt_size_t power_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return 0;
}

static rt_err_t power_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    if (dev == NULL)
    {
        __inf("power_control  dev == NULL");
        while (1);
    }

    switch (cmd)
    {

        case DRV_POWER_CMD_GET_FUELGUAGE:
        {
            __drv_power_fuelguage_t *pbat = (__drv_power_fuelguage_t *)args;
            rt_memcpy(pbat, fuelguage, sizeof(__drv_power_fuelguage_t));
            ret = EPDK_OK;
            break;
        }

        case DRV_POWER_CMD_GET_DCIN:
        {
            gpio_pull_status_t pull_state;
            __drv_power_battery_status_t3 *pstatus = (__drv_power_battery_status_t3 *)args;
            rt_memset(pstatus, 0x00, sizeof(__drv_power_battery_status_t3));
            pstatus->charge_status = fuelguage->charge_status;
            __inf("pstatus->charge_status = %x", pstatus->charge_status);
            return EPDK_OK;
        }
        case DRV_POWER_CMD_RELEASE_DEV:
        {
            init_flag = 0;
            break;
        }
    }
    return ret;
}

static int init_power_device(struct rt_device *dev)
{
    rt_err_t ret = -1;

    if (!dev)
    {
        return ret;
    }
    dev->init        = power_init;
    dev->open        = power_open;
    dev->close       = power_close;
    dev->read        = power_read;
    dev->write       = power_write;
    dev->control     = power_control;

    ret = rt_device_register(dev, "power", RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        software_break();
        return ret;
    }
    return ret;
}

int init_adc_power(void)
{
    struct rt_device *device;

    device = rt_device_create(RT_Device_Class_Char, 0);
    if (!device)
    {
        __wrn("init_adc_power err");
        return -1;
    }
    init_power_device(device);
    return 0;
}
device_initcall(init_adc_power);

int power_info(int argc, char **argv)
{
    char *chargeflag[2] = {"not charging", "charging"};
    __log("battery state : %s", chargeflag[fuelguage->charge_status]);
    __log("battery level   : %d%", fuelguage->rest_vol);
    __log("bat_power val   : %dmv", fuelguage->bat_power);
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(power_info, __cmd_power_info, power_info);
