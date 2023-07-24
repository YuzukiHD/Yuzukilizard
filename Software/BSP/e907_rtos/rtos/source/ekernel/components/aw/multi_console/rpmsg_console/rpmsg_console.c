#include <stdlib.h>
#include <stdint.h>
#include <hal_queue.h>
#include <cli_console.h>
#include <log.h>
#include <hal_uart.h>
#include "rpmsg_console.h"

#ifdef DEBUG
#define log					printf
#else
#define log(...)			do { } while(0)
#endif

static const char *cmd_str[] = {
	"open", "close", "write",
};

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	int i;
	unsigned int value;
	struct rpmsg_shell *shell = ept->priv;
	struct rpmsg_packet *pack = data;

	if (len < sizeof(struct rpmsg_message)) {
		log("%s Received(len:%d)\n", ept->name, len);
		return 0;
	}

	if (!pack->msg.data_length) {
		log("empty message\r\n");
		return 0;
	} else
		log("%s:data=%s\r\n", cmd_str[pack->msg.command], pack->data);

	for (i = 0; i < pack->msg.data_length; i++) {
		value = pack->data[i];
		hal_mailbox_send(shell->mb_rx, value);
	}

	return 0;
}

static int rpmsg_console_write(const void *buf, size_t len, void *priv)
{
	int i;
	int remain;
	const uint8_t *data = buf;
	struct rpmsg_shell *shell = (struct rpmsg_shell *)(priv);

	if (len < RPMSG_MAX_LEN)
		openamp_rpmsg_send(shell->ept, (void *)buf, len);
	else {
		remain = len;
		for (i = 0; i < len;) {
			if (remain > RPMSG_MAX_LEN) {
				openamp_rpmsg_send(shell->ept, (void *)&data[i], RPMSG_MAX_LEN);
				i += RPMSG_MAX_LEN;
				remain -= RPMSG_MAX_LEN;
			} else {
				openamp_rpmsg_send(shell->ept, (void *)&data[i], remain);
				i += remain;
				remain = 0;
			}
		}
	}

	return len;
}

static int rpmsg_console_read(void *buf, size_t len, void *priv)
{
	int i, ret;
	unsigned int value;
	char *data = buf;
	struct rpmsg_shell *shell = (struct rpmsg_shell *)(priv);

	for (i = 0; i < len; i++) {
		ret = hal_mailbox_recv(shell->mb_rx, &value, -1);
		if (!ret)
			data[i] = (char)value;
	}
	return i + 1;
}

static int rpmsg_console_init(void *priv)
{
	struct rpmsg_shell *shell = priv;

	log("rpmsg_console init\r\n");

	return 1;
}

static int rpmsg_console_deinit(void *priv)
{
	struct rpmsg_shell *shell = priv;

	log("rpmsg_console close\r\n");

	return 1;
}

struct rpmsg_shell *
rpmsg_console_create(struct rpmsg_endpoint *ept, uint32_t id)
{
	int ret;
	struct rpmsg_shell *shell = NULL;

	shell = hal_malloc(sizeof(*shell));
	if (!shell)
		return NULL;

	shell->mb_rx = hal_mailbox_create("console-mb-rx", 1024);
	if (!shell->mb_rx) {
		rpmsg_err("create mb_rx failed\r\n");
		return NULL;
	}
	ret = snprintf(shell->name, 32, "rpmsg%ld-console", id);
	if (ret < 0) {
		rpmsg_err("snprintf console name failed!\r\n");
		hal_mailbox_delete(shell->mb_rx);
		return NULL;
	}

	ept->cb           = rpmsg_ept_callback;
	ept->priv         = shell;
	shell->ept        = ept;
	shell->raw.name   = ept->name;
	shell->raw.write  = rpmsg_console_write;
	shell->raw.read   = rpmsg_console_read;
	shell->raw.init   = rpmsg_console_init;
	shell->raw.deinit = rpmsg_console_deinit;

	shell->console = cli_console_create(&shell->raw, shell->name, shell);
	if (!cli_console_check_invalid(shell->console))
		return NULL;

	cli_console_task_create(shell->console, &shell->cli_task, 16 * 1024, 25);

	return shell;
}

void rpmsg_console_delete(struct rpmsg_shell *shell)
{
	cli_console_deinit(shell->console);
	cli_console_destory(shell->console);

	hal_mailbox_delete(shell->mb_rx);
	hal_free(shell);
}
