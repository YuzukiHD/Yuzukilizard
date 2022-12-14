#include <cli_console.h>
#include <stdint.h>
#include <kconfig.h>
#include <sunxi_drv_uart.h>

static rt_device_t fd;

int uart_console_write(const void *buf, size_t len, void *privata_data)
{
    char *output_buf = (char *)buf;
    int i = 0;
    char data;

    while (i < len)
    {
        if (*(output_buf + i) == '\n')
        {
            data = '\r';
            rt_device_write((rt_device_t)fd, 0, &data, 1);
        }
        rt_device_write((rt_device_t)fd, 0, output_buf + i, 1);

        i++;
    }
    return i;
}

/*TODO:  because getchar need 1024 bytes return when going through _read_r, change len = 1*/
int uart_console_read(void *buf, size_t len, void *privata_data)
{
    rt_device_t fd = rt_console_get_device();
    return rt_device_read((rt_device_t)fd, 0, buf, len);
}

static int uart_console_init(void *private_data)
{
    int uart_port = CONFIG_CLI_UART_PORT;
    _uart_config_t uart_config;
    char *uart_name = "uartn";

    uart_config.baudrate    = UART_BAUDRATE_115200;
    uart_config.word_length = UART_WORD_LENGTH_8;
    uart_config.stop_bit    = UART_STOP_BIT_1;
    uart_config.parity      = UART_PARITY_NONE;

#ifdef CONFIG_DRVIER_SYSCONFIG
    if (esCFG_GetKeyValue("uart_para", "uart_debug_port", (int32_t *)&uart_port, 1) != EPDK_OK)
    {
        uart_port = 0;
    }
#endif

    sprintf(uart_name, "uart%d", uart_port);

    fd = rt_device_find(uart_name);
    rt_device_open(fd, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);

    rt_device_control(fd, 0, &uart_config);

    return 1;
}

static int uart_console_deinit(void *private_data)
{
    return 1;
}

static device_console uart_console =
{
    .name       = "uart-console",
    .write      = uart_console_write,
    .read       = uart_console_read,
    .init       = uart_console_init,
    .deinit     = uart_console_deinit
};

cli_console cli_uart_console =
{
    .i_list         = {0},
    .name           = "cli-uart",
    .dev_console    = &uart_console,
    .init_flag      = 0,
    .exit_flag      = 0,
    .alive          = 1,
    .private_data   = NULL,
    .task_list      = {0},
    .finsh_callback = NULL,
    .start_callback = NULL,
};
