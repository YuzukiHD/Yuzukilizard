#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <aw_list.h>
#include <hal_osal.h>
#include <hal_queue.h>
#include <openamp/sunxi_helper/openamp.h>

#ifdef CONFIG_RPMSG_CLIENT_DEBUG
#define log					printf
#else
#define log					hal_log_debug
#endif

static hal_mutex_t g_list_lock = NULL;
static LIST_HEAD(g_test_epts_list);

struct ept_test_entry {
	struct rpmsg_ept_client *client;
	struct list_head list;
};

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	char *pdata = data;
	struct ept_test_entry *eptdev = ept->priv;

	pdata[len] = '\0';

	log("rpmsg%d: Rx %d Bytes\r\n", (int)eptdev->client->id, len);
	log("Data:%s\r\n", pdata);

	return 0;
}

int rpmsg_bind_cb(struct rpmsg_ept_client *client)
{
	struct ept_test_entry *eptdev;

	log("rpmsg%ld: binding\r\n", client->id);

	eptdev = hal_malloc(sizeof(*eptdev));
	if (!eptdev) {
		hal_log_err("failed to alloc client entry\r\n");
		return -ENOMEM;
	}

	eptdev->client = client;
	client->ept->priv = eptdev;
	client->priv = eptdev;

	hal_mutex_lock(g_list_lock);
	list_add(&eptdev->list, &g_test_epts_list);
	hal_mutex_unlock(g_list_lock);

	return 0;
}

int rpmsg_unbind_cb(struct rpmsg_ept_client *client)
{
	struct ept_test_entry *eptdev = client->priv;

	log("rpmsg%ld: unbinding\r\n", client->id);

	hal_mutex_lock(g_list_lock);
	list_del(&eptdev->list);
	hal_mutex_unlock(g_list_lock);

	hal_free(eptdev);

	return 0;
}

static int cmd_rpmsg_eptdev_bind(int argc, char *argv[])
{
	char name[32];
	int len, cnt;

	if (!g_list_lock) {
	g_list_lock = hal_mutex_create();
		if (!g_list_lock) {
			hal_log_err("failed to alloc rpmsg list lock\r\n");
			return -ENOMEM;
		}
	}

	if (argc != 3) {
		printf("Usage: eptdev_listen name cnt\r\n");
		return 0;
	}

	len = strlen(argv[1]);
	if (len >= 32) {
		printf("name too long,shoule less than 32\r\n");
		return -EINVAL;
	}

	cnt = atoi(argv[2]);
	if (cnt <= 0) {
		printf("Invalid id\r\n");
		return -EINVAL;
	}
	memcpy(name, argv[1], len);
	name[len] = '\0';

	rpmsg_client_bind(name, rpmsg_ept_callback, rpmsg_bind_cb,
					rpmsg_unbind_cb, cnt, NULL);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_eptdev_bind, eptdev_bind, bind rpmsg ept name);

static int rpmsg_eptdev_unbind(int argc, char *argv[])
{
	int len;
	char name[32];

	if (argc != 2) {
		printf("Usage: eptdev_unbind name\r\n");
		return 0;
	}

	len = strlen(argv[1]);
	if (len >= 32) {
		printf("name too long,shoule less than 32\r\n");
		return -EINVAL;
	}

	memcpy(name, argv[1], len);
	name[len] = '\0';

	printf("unbind %s\r\n", name);
	rpmsg_client_unbind(name);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(rpmsg_eptdev_unbind, eptdev_unbind, unbind rpmsg ept listen);

static int rpmsg_eptdev_clear(int argc, char *argv[])
{
	int len;
	char name[32];

	if (argc != 2) {
		printf("Usage: eptdev_clear name\r\n");
		return 0;
	}

	len = strlen(argv[1]);
	if (len >= 32) {
		printf("name too long,shoule less than 32\r\n");
		return -EINVAL;
	}

	memcpy(name, argv[1], len);
	name[len] = '\0';

	printf("clear %s group\r\n", name);
	rpmsg_client_clear(name);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(rpmsg_eptdev_clear, eptdev_clear, clear rpmsg name group);

static int rpmsg_eptdev_reset(int argc, char *argv[])
{
	printf("reset rpmsg_ctrl\r\n");
	rpmsg_client_reset();

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(rpmsg_eptdev_reset, eptdev_reset, reset rpmsg ctrl);

