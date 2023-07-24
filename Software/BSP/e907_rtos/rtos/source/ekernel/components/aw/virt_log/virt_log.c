#include <stdio.h>
#include <sunxi_hal_common.h>
#include <hal_interrupt.h>
#include <hal_uart.h>
#include <string.h>
#include <rtthread.h>

#define VIRT_LOG_SIZE				(CONFIG_VIRT_LOG_SIZE * 1024)

static char virt_log_buffer[VIRT_LOG_SIZE];
static uint32_t log_offset = 0;
static uint8_t log_enable = 1;

void virt_log_put(char ch)
{
	unsigned long flags;

	if (!log_enable)
		return;
	/* currently only the sigle-core case is considered */
	flags = hal_interrupt_save();
	virt_log_buffer[log_offset] = ch;
	log_offset++;
	if (log_offset >= VIRT_LOG_SIZE)
		log_offset = 0;
	hal_interrupt_restore(flags);
}

int virt_log_put_buf(char *buf)
{
	int len, remain, writed;
	unsigned long flags;

	if (!log_enable)
		return 0;

	flags = hal_interrupt_save();

	len = strlen(buf);
	if (len > VIRT_LOG_SIZE) {
		buf += (len - VIRT_LOG_SIZE);
		len -= (len - VIRT_LOG_SIZE);
	}
	writed = len;

	/* currently only the sigle-core case is considered */
	remain = VIRT_LOG_SIZE - log_offset;
	if (len > remain) {
		memcpy(&virt_log_buffer[log_offset], buf, remain);
		log_offset = 0;
		len -= remain;
		buf += remain;
	}
	memcpy(&virt_log_buffer[log_offset], buf, len);
	log_offset += len;
	if (log_offset >= VIRT_LOG_SIZE)
		log_offset = 0;
	hal_interrupt_restore(flags);

	return writed;
}

int virt_log_put_buf_len(char *buf, int len)
{
	int remain, writed;
	unsigned long flags;

	if (!log_enable)
		return 0;

	flags = hal_interrupt_save();

	if (len > VIRT_LOG_SIZE) {
		buf += (len - VIRT_LOG_SIZE);
		len -= (len - VIRT_LOG_SIZE);
	}
	writed = len;

	/* currently only the sigle-core case is considered */
	remain = VIRT_LOG_SIZE - log_offset;
	if (len > remain) {
		memcpy(&virt_log_buffer[log_offset], buf, remain);
		log_offset = 0;
		len -= remain;
		buf += remain;
	}
	memcpy(&virt_log_buffer[log_offset], buf, len);
	log_offset += len;
	if (log_offset >= VIRT_LOG_SIZE)
		log_offset = 0;
	hal_interrupt_restore(flags);

	return writed;
}

static void dmesg_putc(char ch)
{
#ifdef CONFIG_SUBSYS_MULTI_CONSOLE
void *get_current_console(void);
int cli_console_write(void *console, const void *buf, size_t nbytes);
	cli_console_write(get_current_console(), &ch, 1);
#elif CONFIG_CLI_UART_PORT
	hal_uart_put_char(CONFIG_CLI_UART_PORT, ch);
#endif
}

static int dmesg(int argc, const char **argv)
{
	uint32_t off, offset, i, j, len;
	char temp[256];
	unsigned long flags;

	flags = hal_interrupt_save();
	log_enable = 0;
	offset = log_offset;

	j = offset;
	while (j < VIRT_LOG_SIZE) {
		len = VIRT_LOG_SIZE - j;
		len = len > sizeof(temp) ? sizeof(temp) : len;

		memcpy(temp, &virt_log_buffer[j], len);
		j += len;

		log_enable = 1;
		hal_interrupt_restore(flags);
		for (i = 0; i < len; i++) {
			if (temp[i] == '\n')
				dmesg_putc('\r');
			dmesg_putc(temp[i]);
		}
		flags = hal_interrupt_save();
		log_enable = 0;
	}

	j = 0;
	while (j < offset) {
		len = offset - j;
		len = len > sizeof(temp) ? sizeof(temp) : len;

		memcpy(temp, &virt_log_buffer[j], len);
		j += len;

		log_enable = 1;
		hal_interrupt_restore(flags);
		for (i = 0; i < len; i++) {
			if (temp[i] == '\n')
				dmesg_putc('\r');
			dmesg_putc(temp[i]);
		}
		flags = hal_interrupt_save();
		log_enable = 0;
	}

	log_enable = 1;
	hal_interrupt_restore(flags);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(dmesg, dmesg, dump log info);
