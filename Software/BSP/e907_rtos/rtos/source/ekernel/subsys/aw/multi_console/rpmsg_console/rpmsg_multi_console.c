#include <stdlib.h>
#include <stdint.h>
#include <hal_queue.h>
#include <cli_console.h>
#include <log.h>
#include <melis/init.h>

#include "rpmsg_console.h"

#define RPMSG_BIND_NAME				"console"
#define RPMSG_CONSOLE_MAX			100

#define log					printf

static hal_mutex_t g_list_lock;
static LIST_HEAD(g_list);

static void rpmsg_create_console(void *arg)
{
	struct rpmsg_service *ser;
	struct rpmsg_ept_client *client = arg;

	ser = hal_malloc(sizeof(*ser));
	if (!ser) {
		hal_log_err("failed to alloc client entry\r\n");
		return;
	}

	ser->client = client;
	client->priv = ser;

	ser->shell = rpmsg_console_create(client->ept, client->id);

	hal_mutex_lock(g_list_lock);
	list_add(&ser->list, &g_list);
	hal_mutex_unlock(g_list_lock);
}

static int rpmsg_bind_cb(struct rpmsg_ept_client *client)
{
	struct rpmsg_service *ser;
	void *thread;

	log("rpmsg%ld: binding\r\n", client->id);

	thread = kthread_create(rpmsg_create_console, client,
					"init", 8 * 1024, 5);
	if (thread != NULL) {
		kthread_start(thread);
		return 0;
	} else
		return -EFAULT;
}

static int rpmsg_unbind_cb(struct rpmsg_ept_client *client)
{
	struct rpmsg_service *ser = client->priv;

	log("rpmsg%ld: unbinding\r\n", client->id);

	hal_mutex_lock(g_list_lock);
	list_del(&ser->list);
	hal_mutex_unlock(g_list_lock);

	rpmsg_console_delete(ser->shell);

	hal_free(ser);

	return 0;
}

static int rpmsg_multi_console_init(void)
{
	g_list_lock = hal_mutex_create();
	if (!g_list_lock) {
		hal_log_err("failed to alloc mutex\r\n");
		return -ENOMEM;
	}

	rpmsg_client_bind(RPMSG_BIND_NAME, NULL, rpmsg_bind_cb,
					rpmsg_unbind_cb, RPMSG_CONSOLE_MAX, NULL);

	return 0;
}

static void rpmsg_multi_console_init_thread(void *arg)
{
	rpmsg_multi_console_init();
}

int rpmsg_multi_console_init_async(void)
{
	void *thread;

	thread = kthread_create(rpmsg_multi_console_init_thread, NULL,
					"init", 8 * 1024, 5);
	if (thread != NULL)
		kthread_start(thread);

	return 0;
}

void rpmsg_multi_console_close(void)
{
	hal_mutex_delete(g_list_lock);
	rpmsg_client_unbind(RPMSG_BIND_NAME);
}
