#include "rpmsg_master.h"
#include <melis/init.h>

#define RPMSG_SERVICE_NAME "sunxi,rpmsg_ctrl"

#ifdef CONFIG_RPMSG_CLIENT_DEBUG
#define log					printf
#else
#define log(...)			do {} while(0)
#endif

static const char *ept_name = RPMSG_SERVICE_NAME;
static uint32_t src_addr = RPMSG_ADDR_ANY;
static uint32_t dst_addr = RPMSG_ADDR_ANY;

struct rpmsg_monitor_entry {
	char name[RPMSG_MAX_NAME_LEN];
	struct list_head list;
	struct list_head epts;
	hal_mutex_t lock;
	uint16_t listen;
	uint16_t alive;
	void *priv;
	rpmsg_ept_cb cb;
	rpmsg_func_cb bind;
	rpmsg_func_cb unbind;
};

struct rpmsg_ept_client_internal {
	struct rpmsg_ept_client client;
	uint32_t ctrl_id;
	struct list_head list;
	struct list_head monitor;
	rpmsg_func_cb bind;
	rpmsg_func_cb unbind;
};

struct rpmsg_ept_ctrldev {
	struct rpmsg_endpoint *ept;
	int is_init;
	hal_mutex_t list_lock;
	hal_thread_t daemon;
	hal_queue_t rx_queue;
	struct list_head listen_list;
	struct list_head epts_list;
};

static struct rpmsg_ept_ctrldev ctrldev;

/*********************************** monitor list function ******************************************/
static void rpmsg_monitor_add_client(struct rpmsg_monitor_entry *monitor,
				struct rpmsg_ept_client_internal *client)
{
	hal_mutex_lock(monitor->lock);
	list_add(&client->monitor, &monitor->epts);
	hal_mutex_unlock(monitor->lock);
}

static void rpmsg_monitor_del_client(struct rpmsg_monitor_entry *monitor,
				struct rpmsg_ept_client_internal *client)
{
	hal_mutex_lock(monitor->lock);
	list_del(&client->monitor);
	hal_mutex_unlock(monitor->lock);
}


/*********************************** epts_list function ******************************************/
static void rpmsg_ctrl_add_client(struct rpmsg_ept_client_internal *client)
{
	hal_mutex_lock(ctrldev.list_lock);
	list_add(&client->list, &ctrldev.epts_list);
	hal_mutex_unlock(ctrldev.list_lock);
}

static void rpmsg_ctrl_del_client(struct rpmsg_ept_client_internal *client)
{
	hal_mutex_lock(ctrldev.list_lock);
	list_del(&client->list);
	hal_mutex_unlock(ctrldev.list_lock);
}

static struct rpmsg_ept_client_internal*
rpmsg_ctrl_find_client_by_id(uint32_t id)
{
	struct rpmsg_ept_client_internal *pos, *tmp;

	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.epts_list, list) {
		if (pos->client.id == id) {
			hal_mutex_unlock(ctrldev.list_lock);
			return pos;
		}
	}

	hal_mutex_unlock(ctrldev.list_lock);

	return NULL;
}

static struct rpmsg_ept_client_internal *
rpmsg_ctrl_find_client_by_ept(struct rpmsg_endpoint *ept)
{
	struct rpmsg_ept_client_internal *pos, *tmp;

	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.epts_list, list) {
		if (pos->client.ept == ept) {
			hal_mutex_unlock(ctrldev.list_lock);
			return pos;
		}
	}

	hal_mutex_unlock(ctrldev.list_lock);

	return NULL;
}

/*********************************** listen_list function ******************************************/
static void rpmsg_ctrl_add_listen(struct rpmsg_monitor_entry *monitor)
{
	hal_mutex_lock(ctrldev.list_lock);
	list_add(&monitor->list, &ctrldev.listen_list);
	hal_mutex_unlock(ctrldev.list_lock);
}

static struct rpmsg_monitor_entry *
rpmsg_ctrl_find_listen_by_name(const char *name)
{
	struct rpmsg_monitor_entry *pos, *tmp;

	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.listen_list, list) {
		if (!strncmp(pos->name, name, RPMSG_MAX_NAME_LEN)) {
			hal_mutex_unlock(ctrldev.list_lock);
			return pos;
		}
	}

	hal_mutex_unlock(ctrldev.list_lock);

	return NULL;
}

/**
 * it will delete tht instance directly.
 * */
static int rpmsg_ctrl_remove_listen_by_name(const char *name)
{
	struct rpmsg_monitor_entry *pos, *tmp;

	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.listen_list, list) {
		if (!strncmp(pos->name, name, RPMSG_MAX_NAME_LEN)) {
			hal_mutex_unlock(ctrldev.list_lock);
			list_del(&pos->list);
			return 0;
		}
	}

	hal_mutex_unlock(ctrldev.list_lock);

	return -ENOENT;
}

static void rpmsg_ctrl_del_listen_by_entry(struct rpmsg_monitor_entry *instance)
{
	hal_mutex_lock(ctrldev.list_lock);
	list_del(&instance->list);
	hal_mutex_unlock(ctrldev.list_lock);
}

static struct rpmsg_ept_client_internal *
rpmsg_ctrl_alloc_client(struct rpmsg_ctrl_msg *msg, struct rpmsg_monitor_entry *monitor)
{
	struct rpmsg_ept_client_internal *entry;

	entry = hal_malloc(sizeof(*entry));
	if (!entry) {
		hal_log_err("failed to alloc client\r\n");
		return NULL;
	}

	entry->client.ept = rpmsg_eptdev_create_client(msg, monitor->cb, NULL, NULL);
	if (!entry->client.ept)
		goto failed;

	strncpy(entry->client.name, msg->name, RPMSG_MAX_NAME_LEN);
	entry->ctrl_id = msg->ctrl_id;
	entry->client.id = msg->id;

	return entry;

failed:
	hal_free(entry);

	return NULL;
}

static void rpmsg_ctrl_free_client(struct rpmsg_ept_client_internal *entry)
{
	rpmsg_eptdev_release_client(entry->client.ept);
	hal_free(entry);
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct rpmsg_ept_ctrldev *ctrl = priv;
	struct rpmsg_ctrl_msg *msg = data;

	log("ctrldev: Rx %d Bytes\n", len);

	if (len != sizeof(*msg))
		return 0;

	hal_queue_send(ctrl->rx_queue, msg);

	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	struct rpmsg_console_daemon *daemon = ept->priv;

	printf("%s endpoint is destroyed\n", ept->name);
}

static void rpmsg_ctrldev_notify(struct rpmsg_ept_ctrldev *ctrl, int id, uint32_t notify)
{
	struct rpmsg_ctrl_msg_ack ack;

	ack.id = id;
	ack.ack = notify;
	rpmsg_send(ctrl->ept, &ack, sizeof(ack));

	log("send 0x%lx to rpmsg%d\r\n", notify, id);
}

static int rpmsg_ctrl_create_client(struct rpmsg_ept_ctrldev *ctrl,
				struct rpmsg_ctrl_msg *msg)
{
	int ret;
	struct rpmsg_ept_client_internal *entry;
	struct rpmsg_monitor_entry *monitor;

	monitor = rpmsg_ctrl_find_listen_by_name(msg->name);
	if (!monitor) {
		log("%s unbind!\r\n", msg->name);
		rpmsg_ctrldev_notify(ctrl, msg->id, RPMSG_ACK_NOLISTEN);
		return -ENOENT;
	}

	if (monitor->listen == monitor->alive) {
		hal_log_warn("listen is full.\r\n");
		rpmsg_ctrldev_notify(ctrl, msg->id, RPMSG_ACK_NOENT);
		return -EBUSY;
	}

	entry = rpmsg_ctrl_alloc_client(msg, monitor);
	if (!entry) {
		rpmsg_ctrldev_notify(ctrl, msg->id, RPMSG_ACK_NOMEM);
		return -ENOMEM;
	}

	entry->client.priv = monitor->priv;
	entry->bind = monitor->bind;
	entry->unbind = monitor->unbind;
	ret = monitor->bind(&entry->client);
	if (ret) {
		hal_log_err("call bind cb failed,ret=%d\r\n", ret);
		goto bind_failed;
	}

	monitor->alive++;
	rpmsg_monitor_add_client(monitor, entry);

	/* add client to ept list */
	rpmsg_ctrl_add_client(entry);

	/* notify remote create success */
	rpmsg_ctrldev_notify(ctrl, msg->id, RPMSG_ACK_OK);

	log("create rpmsg%ld client success\r\n", msg->id);

	return 0;

bind_failed:
	rpmsg_ctrl_free_client(entry);

	return ret;
}

/* rpmsg_ctrl_release_client called by remote,need send ack */
static void rpmsg_ctrl_release_client(struct rpmsg_ept_ctrldev *ctrl, struct rpmsg_ctrl_msg *msg)
{
	struct rpmsg_ept_client_internal *entry;
	struct rpmsg_monitor_entry *monitor;

	entry = rpmsg_ctrl_find_client_by_id(msg->id);
	if (!entry) {
		hal_log_warn("client not exist,id=%d\n", msg->id);
		return;
	}

	rpmsg_ctrldev_notify(ctrl, msg->id, RPMSG_ACK_OK);

	monitor = rpmsg_ctrl_find_listen_by_name(msg->name);
	/* maybe this listen already unbind? */
	if (monitor) {
		monitor->alive--;
		rpmsg_monitor_del_client(monitor, entry);
	}

	rpmsg_ctrl_del_client(entry);

	entry->unbind(&entry->client);

	rpmsg_ctrl_free_client(entry);
}

static void rpmsg_ctrldev_daemon_thread(void *arg)
{
	struct rpmsg_ept_ctrldev *ctrl = arg;
	struct rpmsg_ctrl_msg msg;
	struct rpmsg_monitor_entry *monitor;
	int ret;

	while(1) {
		ret = hal_queue_recv(ctrl->rx_queue, &msg, 50);

		if (ctrl->is_init != 1)
			break;

		if (ret != 0)
			continue;

		if (msg.cmd == RPMSG_CREATE_CLIENT)
			rpmsg_ctrl_create_client(ctrl, &msg);
		else if (msg.cmd == RPMSG_CLOSE_CLIENT)
			rpmsg_ctrl_release_client(ctrl, &msg);
		else if (msg.cmd == RPMSG_RELEASE)
			break;
	}
}

int rpmsg_client_bind(const char *name, rpmsg_ept_cb cb, rpmsg_func_cb bind,
				rpmsg_func_cb unbind, uint32_t cnt, void *priv)
{
	struct rpmsg_monitor_entry *monitor;

	if (cnt == 0) {
		hal_log_warn("Invalid arg:cnt need > 0\r\n");
		return -EINVAL;
	}

	monitor = rpmsg_ctrl_find_listen_by_name(name);
	if (monitor) {
		hal_log_warn("%s already listen.\r\n", name);
		return -EPERM;
	}

	monitor = hal_malloc(sizeof(*monitor));
	if (!monitor) {
		hal_log_err("failed to alloc rpmsg_ept_client\r\n");
		return -ENOMEM;
	}

	monitor->lock = hal_mutex_create();
	if (!monitor->lock) {
		hal_log_err("failed to alloc client\r\n");
		hal_free(monitor);
		return -ENOMEM;
	}

	strncpy(monitor->name, name, RPMSG_MAX_NAME_LEN);
	monitor->cb = cb;
	monitor->bind = bind;
	monitor->unbind = unbind;
	monitor->listen = cnt;
	monitor->alive = 0;
	monitor->priv = priv;
	INIT_LIST_HEAD(&monitor->epts);

	/* add to monitor list */
	rpmsg_ctrl_add_listen(monitor);

	return 0;
}

void rpmsg_client_unbind(const char *name)
{
	struct rpmsg_monitor_entry *monitor;
	struct rpmsg_ept_client_internal *pos, *tmp;

	monitor = rpmsg_ctrl_find_listen_by_name(name);
	if (!monitor) {
		hal_log_warn("%s listen not exist!\r\n");
		return;
	}

	rpmsg_ctrl_del_listen_by_entry(monitor);

	/* release all child */
	hal_mutex_lock(monitor->lock);
	list_for_each_entry_safe(pos, tmp, &monitor->epts, monitor) {
		rpmsg_ctrl_del_client(pos);
		pos->unbind(&pos->client);
		rpmsg_eptdev_release_client(pos->client.ept);
		hal_free(pos);
	}
	hal_mutex_unlock(monitor->lock);

	hal_mutex_delete(monitor->lock);
	hal_free(monitor);
}

/* rpmsg_eptldev_close called by slave,not need send ack */
void rpmsg_eptldev_close(struct rpmsg_ept_client *client)
{
	struct rpmsg_ept_client_internal *entry;
	struct rpmsg_monitor_entry *monitor;

	entry = rpmsg_ctrl_find_client_by_id(client->id);
	if (!entry) {
		hal_log_warn("client not exist,id=%d\n", entry->client.id);
		return;
	}

	monitor = rpmsg_ctrl_find_listen_by_name(client->name);
	/* maybe this listen already unbind? */
	if (monitor) {
		monitor->alive--;
		rpmsg_monitor_del_client(monitor, entry);
	}

	rpmsg_ctrl_del_client(entry);

	entry->unbind(&entry->client);

	rpmsg_eptdev_release_client(entry->client.ept);

	hal_free(entry);
}

void rpmsg_ctrldev_release(void)
{
	struct rpmsg_ctrl_msg msg;
	struct rpmsg_monitor_entry *pos, *tmp;

	log("ctrldev: release all Endpoints.\r\n");
	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.listen_list, list)
		rpmsg_client_unbind(pos->name);
	hal_mutex_unlock(ctrldev.list_lock);

	ctrldev.is_init = -1;

	/* wakeup daemon thread */
	msg.cmd = RPMSG_RELEASE;
	hal_queue_send(ctrldev.rx_queue, &msg);
	hal_msleep(10);
	kthread_stop(ctrldev.daemon);

	hal_queue_delete(ctrldev.rx_queue);
	openamp_ept_close(ctrldev.ept);
	hal_mutex_delete(ctrldev.list_lock);
}

int rpmsg_ctrldev_create(void)
{
	int ret = 0;

	if (ctrldev.is_init == 1) {
		log("already create\r\n");
		return -EEXIST;
	}

	ctrldev.list_lock = hal_mutex_create();
	if (!ctrldev.list_lock) {
		hal_log_err("failed to alloc client\r\n");
		return -ENOMEM;
	}

	ctrldev.daemon = kthread_create(rpmsg_ctrldev_daemon_thread, &ctrldev,
					"ctrldev", 1024 * 4, 6);
	if (ctrldev.daemon == NULL) {
		hal_log_err("failed to create ctrldev thread\r\n");
		ret = -ENOMEM;
		goto free_mutex;
	}

	ctrldev.rx_queue = hal_queue_create("rpmsg-ctrl-rx",
					sizeof(struct rpmsg_ctrl_msg), CONFIG_RPMSG_CLIENT_QUEUE_SIZE);
	if (!ctrldev.rx_queue) {
		hal_log_err("failed to create ctrldev rx queue\r\n");
		ret = -ENOMEM;
		goto free_thread;
	}

	INIT_LIST_HEAD(&ctrldev.listen_list);
	INIT_LIST_HEAD(&ctrldev.epts_list);

	hal_log_debug("Wait Remoteproc readdy...\r\n");
	if (openamp_init() != 0) {
		printf("Failed to init openamp framework\r\n");
		ret = -EBUSY;
		goto free_queue;
	}

	hal_log_debug("Creating Ctrldev Endpoint\r\n");
	ctrldev.ept = openamp_ept_open(ept_name, 0, src_addr, dst_addr, &ctrldev,
					rpmsg_ept_callback, rpmsg_unbind_callback);
	if(ctrldev.ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		ret = -EPERM;
		goto free_queue;
	}

	ctrldev.is_init = 1;
	kthread_start(ctrldev.daemon);
	printf("rpmsg ctrldev: Start Running...\n");

	return 0;

free_queue:
	hal_queue_delete(ctrldev.rx_queue);
free_thread:
	kthread_stop(ctrldev.daemon);
free_mutex:
	hal_mutex_delete(ctrldev.list_lock);
out:
	return ret;
}

void rpmsg_ctrldev_init_thread(void *arg)
{


	rpmsg_ctrldev_create();
}

static int rpmsg_ctrldev_init_async(void)
{
	void *thread;

	thread = kthread_create(rpmsg_ctrldev_init_thread, NULL,
					"rpmsg_ctrl", 8 * 1024, 5);
	if (thread != NULL)
		kthread_start(thread);
out:
	return 0;
}
device_initcall(rpmsg_ctrldev_init_async);

#ifdef CONFIG_RPMSG_CLIENT_DEBUG
static int cmd_rpmsg_ctrl_thread(int argc, char *argv[])
{
	void *thread;

	thread = kthread_create(rpmsg_ctrldev_init_thread, NULL,
					"rpmsg_ctrl", 8 * 1024, 5);
	if (thread != NULL)
		kthread_start(thread);
out:
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_ctrl_thread, rpmsg_ctrldev_init, init rpmsg ctrldev);

static int cmd_rpmsg_ctrldev_close(int argc, char *argv[])
{
	rpmsg_ctrldev_release();
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_ctrldev_close, rpmsg_ctrlde_release, release rpmsg ctrldev);

static int cmd_enable_jtag(int argc, char *argv[])
{
	uint32_t reg_val;

	reg_val = *(volatile uint32_t *)(0x02000000 + 0x0154);
	reg_val &= ~(0xffff << 3 * 4);
	reg_val |= (0x4444 << 3 * 4);
	*(volatile uint32_t *)(0x02000000 + 0x0154) = reg_val;

	hal_msleep(10);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_enable_jtag, jtag, init jtag gpio);

static int cmd_list_listen(int argc, char *argv[])
{
	struct rpmsg_monitor_entry *pos, *tmp;

	printf("name \t\t listen  alive\r\n");
	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.listen_list, list) {
		printf("%s \t\t %d  %d\r\n", pos->name, pos->listen, pos->alive);
	}
	hal_mutex_unlock(ctrldev.list_lock);
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_list_listen, rpmsg_list_listen, list listen);

static int cmd_list_epts(int argc, char *argv[])
{
	struct rpmsg_ept_client_internal *pos, *tmp;

	printf("name \t\t major \t\t src - dst\r\n");
	hal_mutex_lock(ctrldev.list_lock);
	list_for_each_entry_safe(pos, tmp, &ctrldev.epts_list, list) {
		printf("rpmsg%ld \t\t %ld \t\t 0x%2lx - 0x%2lx\r\n", pos->client.id,
						pos->ctrl_id, pos->client.ept->addr,  pos->client.ept->dest_addr);
	}
	hal_mutex_unlock(ctrldev.list_lock);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_list_epts, rpmsg_list_epts, list endpoints);

static int cmd_rpmsg_eptdev_close(int argc, char *argv[])
{
	int id;
	struct rpmsg_ept_client_internal *rpmsg;

	if (argc != 2) {
		printf("Usage: eptdev_close id\r\n");
		return 0;
	}

	id = atoi(argv[1]);

	rpmsg = rpmsg_ctrl_find_client_by_id(id);
	if (!rpmsg) {
		printf("rpmsg%d not exists\r\n", id);
		return 0;
	}

	printf("will close rpmsg%d\r\n", id);

	rpmsg_eptldev_close(&rpmsg->client);

out:
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_eptdev_close, eptdev_close, close rpmsg ept client);

static int cmd_rpmsg_eptdev_send(int argc, char *argv[])
{
	int id;
	char *data;
	int len;
	struct rpmsg_ept_client_internal *rpmsg;

	if (argc != 3) {
		printf("Usage: eptdev_send id data,argc=%d\r\n", argc);
		return 0;
	}

	id = atoi(argv[1]);
	data = argv[2];
	len = strlen(data);
	if (len  > 100)
		len = 100;

	rpmsg = rpmsg_ctrl_find_client_by_id(id);
	if (!rpmsg) {
		printf("rpmsg%d not exists\r\n", id);
		return 0;
	}

	printf("will send %s to rpmsg%d\r\n", data, id);

	openamp_rpmsg_send(rpmsg->client.ept, data, len);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_eptdev_send, eptdev_send, send data used by rpmsg ept test);
#endif
