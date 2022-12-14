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
#include <string.h>
#include <stdint.h>
#include <rtthread.h>

#include <sunxi_drv_pwm.h>
#include <sunxi_hal_pwm.h>

static int cmd_drv_pwm(int argc, char **argv)
{

    struct pwm_config *config ;
    uint8_t port;
    uint8_t ns;

    if (argc < 2)
    {
        printf("Usage:drv pwm port\n");
        return -1;
    }

    port = strtol(argv[1], NULL, 0);

    config = (struct pwm_config *)malloc(sizeof(struct pwm_config));
    config->duty_ns   = 3000000;
    config->period_ns = 10000000;
    config->polarity  = PWM_POLARITY_NORMAL;
    printf("port = %d\n", port);
    printf("duty = %d \n", config->duty_ns);
    printf("period = %d\n", config->period_ns);
    printf("polarity = %d\n", config->polarity);

    rt_device_t fd;

    fd = rt_device_find("pwm");

    rt_device_open(fd, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);

    rt_device_init(fd);

    rt_device_control(fd, port, config);

    rt_device_close(fd);

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_drv_pwm, cmd_pwm, drv pwm test code);

static int cmd_pwm(int argc, char **argv)
{

    struct pwm_config *config ;
    uint8_t port;
    uint32_t ns;

    if (argc < 3)
    {
        printf("Usage: cmd_hal_pwm [pwm port] [dust_ns]");
        return -1;
    }

    port = strtol(argv[1], NULL, 0);
    ns   = strtol(argv[2], NULL , 0);

    config = (struct pwm_config *)malloc(sizeof(struct pwm_config));
    config->duty_ns   = ns;
    config->period_ns = 10000000;
    config->polarity  = PWM_POLARITY_NORMAL;

    printf("port = %d\n", port);
    printf("duty = %d \n", config->duty_ns);
    printf("period = %d\n", config->period_ns);
    printf("polarity = %d\n", config->polarity);

    hal_pwm_init();

    hal_pwm_control(port, config);

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_pwm, cmd_hal_pwm, pwm test code);
