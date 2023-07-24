#ifndef _RPMSG_CONSOLE_H_
#define _RPMSG_CONSOLE_H_

#include <hal_queue.h>
#include <openamp/sunxi_helper/openamp.h>

enum
{
	A_OPEN = 0,
	A_CLSE,
	A_WRTE,
};

enum
{
	RPMSG_CONSOLE_SERVICE_TYPE_SHELL = 0,
	RPMSG_CONSOLE_SERVICE_TYPE_SYNC,
};

typedef pthread_t rpmsg_console_thread_t;

struct rpmsg_message {
	unsigned int command;
	unsigned int data_length;
};

struct rpmsg_packet {
	struct rpmsg_message msg;
	char data[512 - 16 - sizeof(struct rpmsg_message)];
};

struct rpmsg_shell {
	struct rpmsg_endpoint *ept;
	device_console raw;
	hal_mailbox_t mb_rx;

	rpmsg_console_thread_t cli_task;
	cli_console *console;

	char name[32];
};

struct rpmsg_service {
	struct rpmsg_shell *shell;
	struct rpmsg_ept_client *client;
	struct list_head list;
};

struct rpmsg_shell *
rpmsg_console_create(struct rpmsg_endpoint *ept, uint32_t id);
void rpmsg_console_delete(struct rpmsg_shell *shell);

#define shell_to_service(p)		container_of(p, struct rpmsg_service, shell)

#define rpmsg_err(fmt, args...) \
    printf("[RPMSG_ERR][%s:%d]" fmt, __FUNCTION__, __LINE__, ##args)

#ifdef RPMSG_DEBUG
#define rpmsg_debug(fmt, args...) \
    printf("[RPMSG_DBG][%s:%d]" fmt, __FUNCTION__, __LINE__, ##args)
#else
#define rpmsg_debug(fmt, args...)
#endif

#ifdef RPMSG_IRQ_DEBUG
#define rpmsg_irq_debug(fmt, args...) \
    printk("[RPMSG_IRQ][%s:%d]" fmt, __FUNCTION__, __LINE__, ##args)
#else
#define rpmsg_irq_debug(fmt, args...)
#endif

#endif
