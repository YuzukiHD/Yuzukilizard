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
#include <getopt.h>
#include <log.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <hal_uart.h>
#include <sunxi_drv_uart.h>
#include <hal_timer.h>

extern int msleep(int);

static void serial_thread_entry(void *parameter)
{
    rt_device_t fd = (rt_device_t)parameter;
    char ch;
    while (1)
    {
	/* only read one data, will block */
	while (rt_device_read(fd, 0, &ch, 1) != 1)
	{
	}
	printf("%s", &ch);
	/* data + 1 and then send */
	/* ch = ch + 1; */
	/* rt_device_write(serial, 0, &ch, 1); */
    }
}

static int cmd_drv_uart(int argc, const char **argv)
{
    rt_device_t fd;
    char buf[1024]={"hello"};
    _uart_config_t uart_config;
    char uart_name[10] ={0};
    uart_port_t port;
    uint32_t baudrate;
    int ret = 0;

    if (argc < 3)
    {
        printk("Usage:\n");
        printk("\tuart <port> <baudrate> \n");
        return -1;
    }

    port = strtol(argv[1], NULL, 0);
    baudrate = strtol(argv[2], NULL, 0);

    switch (baudrate)
    {
	    case 4800:
		    uart_config.baudrate    = UART_BAUDRATE_4800;
		    break;
	    case 9600:
		    uart_config.baudrate    = UART_BAUDRATE_9600;
		    break;
	    case 115200:
		    uart_config.baudrate    = UART_BAUDRATE_115200;
		    break;
	    default:
		    printf("use default baudrate:115200 \n");
		    uart_config.baudrate    = UART_BAUDRATE_115200;
		    break;
    }
    uart_config.word_length = UART_WORD_LENGTH_8;
    uart_config.stop_bit    = UART_STOP_BIT_1;
    uart_config.parity      = UART_PARITY_NONE;

    memset(uart_name, 0, sizeof(uart_name));
    sprintf(uart_name, "uart%d", port);
    fd = rt_device_find(uart_name);
    rt_device_open(fd, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);
    /* rt_device_init((rt_device_t)fd); */
    rt_device_control(fd, 0, &uart_config); //uart control

    /* creat serial thread */
    rt_thread_t thread = rt_thread_create("serial", serial_thread_entry, fd, 1024, 25, 10);
    if (thread != RT_NULL)
	    rt_thread_startup(thread);
    else
	    ret = RT_ERROR;

    rt_device_write((rt_device_t)fd, 0, buf, 5);
    printf("rt_device_write ret = %d\n", ret);
    while(1)
    {
        memset(buf, 0, sizeof(buf));
        scanf("%1023s", buf);
        if(memcmp(buf, "quit", 4) == 0) {
            printf("quit.\r\n");
            break;
        }
        buf[strlen(buf)] = '\r';
        rt_device_write(fd, 0, buf, strlen(buf)+1);
        printf("Send data[%d]:%s\n", strlen(buf)+1, buf);
    }
    /* rt_device_read((rt_device_t)fd, 0, rbuf, 5); */
    /* printf("receive buf :%s\n",rbuf); */

    /* rt_device_close((rt_device_t)fd); */
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_drv_uart, __cmd_drvuart, rtthread uart test code);

#define TEST_READ 0
#define TEST_WRITE 1

static int cmd_hal_uart(int argc, const char **argv)
{
    char tbuf[5]={"hello"};
    int rbuf[10] = {0};
    uart_port_t port;
    uint32_t baudrate;
    char rw = 1;
    int c;
    _uart_config_t uart_config;
    int fd;

    if (argc < 4)
    {
        printk("Usage:\n");
        printk("\tuart <port> <baudrate> <-r|-w> \n");
        return -1;
    }

    port = strtol(argv[1], NULL, 0);
    baudrate = strtol(argv[2], NULL, 0);

    while ((c = getopt(argc, (char *const *)argv, "r:w")) != -1)
    {
        switch (c)
        {
            case 'r':
		printf("UART[%d]:read\n",port);
                rw = TEST_READ;
                break;
            case 'w':
		printf("UART[%d]:write\n",port);
                rw = TEST_WRITE;
                break;
        }
    }

    memset(rbuf, 0, 10 * sizeof(int));
    switch (baudrate)
    {
	    case 4800:
		    uart_config.baudrate    = UART_BAUDRATE_4800;
		    break;
	    case 9600:
		    uart_config.baudrate    = UART_BAUDRATE_9600;
		    break;
	    case 115200:
		    uart_config.baudrate    = UART_BAUDRATE_115200;
		    break;
	    default:
		    printf("use default baudrate:115200 \n");
		    uart_config.baudrate    = UART_BAUDRATE_115200;
		    break;
    }
    uart_config.word_length = UART_WORD_LENGTH_8;
    uart_config.stop_bit    = UART_STOP_BIT_1;
    uart_config.parity      = UART_PARITY_NONE;

    hal_uart_init(port);
    hal_uart_control(port, 0, &uart_config);
    hal_uart_disable_flowcontrol(port);
    /* hal_uart_set_hardware_flowcontrol(port); */

    if (rw == TEST_READ)
    {
        hal_uart_receive(port, rbuf, 5);
	printf("uart[%d] receive buf :%c %c, %c, %c, %c.\n",port, rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4]);
    }
    else if (rw == TEST_WRITE)
    {
        hal_uart_send(port, tbuf, 5);
    }
    msleep(1000);
    hal_uart_deinit(port);

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_hal_uart, __cmd_uart, hal uart test code);

