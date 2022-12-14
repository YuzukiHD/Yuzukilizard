#include <hal/hal_gpio.h>
#include <drv/sunxi_drv_gpio.h>
#include <stdio.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int light_led(int argc, char **argv)
{
    uint32_t irq;
    int ret = 0;
    gpio_pull_status_t pull_state;
    gpio_direction_t gpio_direction;
    gpio_data_t gpio_data;

    hal_gpio_get_pull(GPIO_PH11, &pull_state);
    hal_gpio_get_direction(GPIO_PH11, &gpio_direction);
    hal_gpio_get_data(GPIO_PH11, &gpio_data);
    printf("before pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    hal_gpio_set_driving_level(GPIO_PH11, GPIO_DRIVING_LEVEL3);
    hal_gpio_set_direction(GPIO_PH11, GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_data(GPIO_PH11, GPIO_DATA_HIGH);
    hal_gpio_sel_vol_mode(GPIO_PH11, POWER_MODE_330);
    hal_gpio_set_debounce(GPIO_PH11, 1);
    for(int i=0; i<=10; i++)
    {
    int msleep(unsigned int msecs);
    msleep(100);
    hal_gpio_set_pull(GPIO_PH11, GPIO_PULL_DOWN);
    msleep(100);
    hal_gpio_set_pull(GPIO_PH11, GPIO_PULL_UP);
    }
    hal_gpio_get_pull(GPIO_PH11, &pull_state);
    hal_gpio_get_direction(GPIO_PH11, &gpio_direction);
    hal_gpio_get_data(GPIO_PH11, &gpio_data);

    printf("after pull state : %d, dir : %d, data :0x%0x\n", pull_state, gpio_direction, gpio_data);

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(light_led, __cmd_led, led test code);


