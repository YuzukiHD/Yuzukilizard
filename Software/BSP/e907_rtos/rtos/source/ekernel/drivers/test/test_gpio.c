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
/*how to include the head file*/
#include <hal_gpio.h>
#include <sunxi_drv_gpio.h>
#include <stdio.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static hal_irqreturn_t gpio_irq_test(void *data)
{
    printf("run interrupt handle\n");
    return 0;
}


int cmd_gpio(int argc, char **argv)
{
    uint32_t irq;
    int ret = 0;
    gpio_pull_status_t pull_state;
    gpio_direction_t gpio_direction;
    gpio_data_t gpio_data;

    hal_gpio_get_pull(GPIO_PE6, &pull_state);
    hal_gpio_get_direction(GPIO_PE6, &gpio_direction);
    hal_gpio_get_data(GPIO_PE6, &gpio_data);
    printf("pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    hal_gpio_set_pull(GPIO_PE6, GPIO_PULL_UP);
    hal_gpio_set_direction(GPIO_PE6, GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_data(GPIO_PE6, GPIO_DATA_HIGH);
    hal_gpio_sel_vol_mode(GPIO_PE6, POWER_MODE_180);
    hal_gpio_sel_vol_mode(GPIO_PF1, POWER_MODE_180);
    hal_gpio_set_debounce(GPIO_PF1, 1);

    hal_gpio_get_pull(GPIO_PE6, &pull_state);
    hal_gpio_get_direction(GPIO_PE6, &gpio_direction);
    hal_gpio_get_data(GPIO_PE6, &gpio_data);

    printf("pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    hal_gpio_get_pull(GPIO_PL2, &pull_state);
    hal_gpio_get_direction(GPIO_PL2, &gpio_direction);
    hal_gpio_get_data(GPIO_PL2, &gpio_data);

    printf("pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    hal_gpio_set_pull(GPIO_PL2, GPIO_PULL_DOWN);
    hal_gpio_set_direction(GPIO_PL2, GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_data(GPIO_PL2, GPIO_DATA_HIGH);
    hal_gpio_sel_vol_mode(GPIO_PL2, POWER_MODE_180);
    hal_gpio_set_debounce(GPIO_PL2, 1);

    hal_gpio_get_pull(GPIO_PL2, &pull_state);
    hal_gpio_get_direction(GPIO_PL2, &gpio_direction);
    hal_gpio_get_data(GPIO_PL2, &gpio_data);

    printf("pull state : %d, dir : %d, data : 0x%0x\n", pull_state, gpio_direction, gpio_data);

    ret = hal_gpio_to_irq(GPIO_PH4, &irq);

    if (ret < 0)
    {
        printf("gpio to irq error, irq num:%u error num: %d\n", irq, ret);
        return ret;
    }

    ret = hal_gpio_irq_enable(irq);
    if (ret < 0)
    {
        printf("request irq error, error num: %d\n", ret);
        return ret;
    }

    ret = hal_gpio_irq_disable(irq);
    if (ret < 0)
    {
        printf("disable irq error, irq num:%u,error num: %d\n", irq, ret);
        return ret;
    }

    ret = hal_gpio_irq_free(irq);
    if (ret < 0)
    {
        printf("free irq error, error num: %d\n", ret);
        return ret;
    }

    ret = hal_gpio_to_irq(GPIO_PL0, &irq);

    if (ret < 0)
    {
        printf("gpio to irq error, irq num:%u error num: %d\n", irq, ret);
        return ret;
    }

    ret = hal_gpio_irq_request(irq, gpio_irq_test, IRQ_TYPE_EDGE_RISING, NULL);
    if (ret < 0)
    {
        printf("request irq error, irq num:%u error num: %d\n", irq, ret);
        return ret;
    }

    ret = hal_gpio_irq_enable(irq);
    if (ret < 0)
    {
        printf("request irq error, error num: %d\n", ret);
        return ret;
    }
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_gpio, __cmd_gpio, hal gpio test code);

int cmd_drv_gpio(int argc, char **argv)
{
    uint32_t irq;
    int ret = 0;
    gpio_pull_status_t pull_state;
    gpio_direction_t gpio_direction;
    gpio_data_t gpio_data;

    drv_gpio_get_pull_status(GPIO_PE6, &pull_state);
    drv_gpio_get_direction(GPIO_PE6, &gpio_direction);
    drv_gpio_get_output(GPIO_PE6, &gpio_data);
    printf("pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    drv_gpio_set_pull_status(GPIO_PE6, GPIO_PULL_UP);
    drv_gpio_set_direction(GPIO_PE6, GPIO_DIRECTION_OUTPUT);
    drv_gpio_set_output(GPIO_PE6, GPIO_DATA_HIGH);

    drv_gpio_sel_vol_mode(GPIO_PE6, POWER_MODE_180);
    drv_gpio_sel_vol_mode(GPIO_PF1, POWER_MODE_180);
    drv_gpio_set_debounce(GPIO_PL2, 1);

    drv_gpio_get_pull_status(GPIO_PE6, &pull_state);
    drv_gpio_get_direction(GPIO_PE6, &gpio_direction);
    drv_gpio_get_output(GPIO_PE6, &gpio_data);

    printf("pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    drv_gpio_get_pull_status(GPIO_PL2, &pull_state);
    drv_gpio_get_direction(GPIO_PL2, &gpio_direction);
    drv_gpio_get_output(GPIO_PL2, &gpio_data);

    printf("pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    drv_gpio_set_pull_status(GPIO_PL2, GPIO_PULL_DOWN);
    drv_gpio_set_direction(GPIO_PL2, GPIO_DIRECTION_OUTPUT);
    drv_gpio_set_output(GPIO_PL2, GPIO_DATA_HIGH);
    drv_gpio_sel_vol_mode(GPIO_PL2, POWER_MODE_180);
    drv_gpio_set_debounce(GPIO_PL2, 1);

    drv_gpio_get_pull_status(GPIO_PL2, &pull_state);
    drv_gpio_get_direction(GPIO_PL2, &gpio_direction);
    drv_gpio_get_output(GPIO_PL2, &gpio_data);

    printf("pull state : %d, dir : %d, data : 0x%0x\n", pull_state, gpio_direction, gpio_data);

    ret = drv_gpio_to_irq(GPIO_PH4, &irq);

    if (ret < 0)
    {
        printf("gpio to irq error, irq num:%u error num: %d\n", irq, ret);
        return ret;
    }

    ret = drv_gpio_irq_enable(irq);
    if (ret < 0)
    {
        printf("request irq error, error num: %d\n", ret);
        return ret;
    }

    ret = drv_gpio_irq_disable(irq);
    if (ret < 0)
    {
        printf("disable irq error, irq num:%u,error num: %d\n", irq, ret);
        return ret;
    }

    ret = drv_gpio_irq_free(irq);
    if (ret < 0)
    {
        printf("free irq error, error num: %d\n", ret);
        return ret;
    }

    ret = drv_gpio_to_irq(GPIO_PL0, &irq);

    if (ret < 0)
    {
        printf("gpio to irq error, irq num:%u error num: %d\n", irq, ret);
        return ret;
    }

    ret = drv_gpio_irq_request(irq, gpio_irq_test, IRQ_TYPE_EDGE_RISING, NULL);
    if (ret < 0)
    {
        printf("request irq error, irq num:%u error num: %d\n", irq, ret);
        return ret;
    }

    ret = drv_gpio_irq_enable(irq);
    if (ret < 0)
    {
        printf("request irq error, error num: %d\n", ret);
        return ret;
    }
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_drv_gpio, __cmd_drvgpio, drv gpio test code);
