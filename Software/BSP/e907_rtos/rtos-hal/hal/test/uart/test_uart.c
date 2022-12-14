/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <hal_log.h>
#include <hal_cmd.h>
#include <hal_timer.h>
#include <hal_uart.h>

static void cmd_usage(void)
{
	printf("Usage:\n"
		"\t hal_uart <port> <baudrate>\n");
}

int cmd_test_uart(int argc, char **argv)
{
	char tbuf[6] = {"hello"};
	uint8_t rbuf[10] = {0};
	uart_port_t port;
	uint32_t baudrate;
	_uart_config_t uart_config;
	int i;

	hal_log_info("Testing UART in loopback mode");

	if (argc != 3) {
		cmd_usage();
		return -1;
	}

	port = strtol(argv[1], NULL, 0);
	baudrate = strtol(argv[2], NULL, 0);

	if(CONFIG_CLI_UART_PORT == port){
		hal_log_info("uart0 can't test, please use other port!");
		return -1;
	}
	memset(rbuf, 0, 10 * sizeof(uint8_t));

	switch (baudrate) {
	case 4800:
		uart_config.baudrate = UART_BAUDRATE_4800;
		break;

	case 9600:
		uart_config.baudrate = UART_BAUDRATE_9600;
		break;

	case 115200:
		uart_config.baudrate = UART_BAUDRATE_115200;
		break;

	default:
		hal_log_info("Using default baudrate: 115200");
		uart_config.baudrate = UART_BAUDRATE_115200;
		break;
	}

	uart_config.word_length = UART_WORD_LENGTH_8;
	uart_config.stop_bit = UART_STOP_BIT_1;
	uart_config.parity = UART_PARITY_NONE;

	hal_uart_init(port);
	hal_uart_control(port, 0, &uart_config);
	hal_uart_disable_flowcontrol(port);
	hal_uart_set_loopback(port, 1);

	/* send */
	hal_uart_send(port, tbuf, 5);

	/* loopback receive */
	hal_uart_receive(port, rbuf, 5);

	printf("Sending:");
	for (i = 0; i < 5; i++)
		printf("%c", tbuf[i]);
	printf("\n");

	printf("Receiving:");
	for (i = 0; i < 5; i++)
		printf("%c", rbuf[i]);
	printf("\n");

	/* verify data */
	for (i = 0; i < 5; i++) {
		if (tbuf[i] != rbuf[i])
			break;
	}
	if (i == 5) {
		hal_log_info("Test hal_uart_init API success!");
		hal_log_info("Test hal_uart_control API success!");
		hal_log_info("Test hal_uart_disable_flowcontrol API success!");
		hal_log_info("Test hal_uart_set_loopback API success!");
		hal_log_info("Test hal_uart_send API success!");
		hal_log_info("Test hal_uart_receive API success!");
		hal_log_info("Test hal_uart_deinit API success!");
		hal_log_info("Test uart hal APIs success!");
	} else {
		hal_log_info("Test uart hal APIs failed!");
	}

	hal_msleep(1000);
	hal_uart_deinit(port);

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_uart, hal_uart, uart hal APIs tests)

#define BUFFSIZE 4096

static void cmd_stress_usage(void)
{
	printf("Usage:\n"
		"\t hal_uart <port> <baudrate> <flowctrl> <loopback> <len>\n");
}

int cmd_test_uart_stress(int argc, char **argv)
{
	uint8_t *tbuf = malloc(BUFFSIZE);
	uint8_t *rbuf = malloc(BUFFSIZE);
	uart_port_t port;
	uint32_t baudrate;
	_uart_config_t uart_config;
	int i;
	int flowctrl, loopback, testlen;

	hal_log_info("Testing UART in loopback mode with stress");

	if (argc != 6) {
		cmd_stress_usage();
		free(tbuf);
		free(rbuf);
		return -1;
	}

	port = strtol(argv[1], NULL, 0);
	baudrate = strtol(argv[2], NULL, 0);
	flowctrl = strtol(argv[3], NULL, 0);
	loopback = strtol(argv[4], NULL, 0);
	testlen = strtol(argv[5], NULL, 0);

	for (i = 0; i < BUFFSIZE; i++) {
		tbuf[i] = ('a' + i) & 0xff;
	}
	memset(rbuf, 0, BUFFSIZE * sizeof(uint8_t));

	switch (baudrate) {
	case 4800:
		uart_config.baudrate = UART_BAUDRATE_4800;
		break;

	case 9600:
		uart_config.baudrate = UART_BAUDRATE_9600;
		break;

	case 115200:
		uart_config.baudrate = UART_BAUDRATE_115200;
		break;

	case 1500000:
		uart_config.baudrate = UART_BAUDRATE_1500000;
		break;

	default:
		hal_log_info("Using default baudrate: 115200");
		uart_config.baudrate = UART_BAUDRATE_115200;
		break;
	}

	uart_config.word_length = UART_WORD_LENGTH_8;
	uart_config.stop_bit = UART_STOP_BIT_1;
	uart_config.parity = UART_PARITY_NONE;

	hal_uart_init(port);
	hal_uart_control(port, 0, &uart_config);
	printf("flow:%d, loopback:%d len:%d\n", flowctrl, loopback, testlen);
	if (flowctrl)
		hal_uart_set_hardware_flowcontrol(port);
	else
		hal_uart_disable_flowcontrol(port);

	if (loopback)
		hal_uart_set_loopback(port, 1);
	else
		hal_uart_set_loopback(port, 0);

	/* send */
	printf("send\n");
	hal_uart_send(port, tbuf, testlen);
	printf("send done\n");

	printf("recv\n");
	/* loopback receive */
	hal_uart_receive(port, rbuf, testlen);
	//hal_uart_receive_no_block(port, rbuf, testlen, 1000);
	printf("recv done\n");

#if 0
	printf("Sending:");
	for (i = 0; i < testlen; i++) {
		if (i % 16 == 0)
			printf("\n");
		printf("0x%x ", tbuf[i]);
	}
	printf("\n");

	printf("Receiving:");
	for (i = 0; i < testlen; i++) {
		if (i % 16 == 0)
			printf("\n");
		printf("0x%x ", rbuf[i]);
	}
	printf("\n");
#endif

	/* verify data */
	for (i = 0; i < testlen; i++) {
		if (tbuf[i] != rbuf[i]) {
			printf("check %d fail, 0x%x != 0x%x\n", i, tbuf[i], rbuf[i]);
			break;
		}
	}
	if (i == testlen) {
		hal_log_info("Test hal_uart_init API success!");
		hal_log_info("Test hal_uart_control API success!");
		hal_log_info("Test hal_uart_disable_flowcontrol API success!");
		hal_log_info("Test hal_uart_set_loopback API success!");
		hal_log_info("Test hal_uart_send API success!");
		hal_log_info("Test hal_uart_receive API success!");
		hal_log_info("Test hal_uart_deinit API success!");
		hal_log_info("Test uart hal APIs success!");
	} else {
		hal_log_info("Test uart hal APIs failed!");
	}

	hal_msleep(1000);
	hal_uart_deinit(port);
	free(tbuf);
	free(rbuf);


	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_uart_stress, hal_uart_stress, uart hal APIs tests)
