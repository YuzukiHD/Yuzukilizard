#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/time.h>

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/msgbox_ipi.h>

#define RPMSG_SERVICE_NAME "sunxi,rpmsg-speed-test"

#define DATA_LEN_PER		(512 - 16)

#if 0
#define speed_debug(fmt, args...)		printf(fmt, ##args)
#else
#define speed_debug(fmt, args...)
#endif

static const char *name = RPMSG_SERVICE_NAME;
static uint32_t src_addr = RPMSG_ADDR_ANY;
static uint32_t dst_addr = RPMSG_ADDR_ANY;
static uint8_t data_send[DATA_LEN_PER];

static int data_len = 2 * 1024 * 4 * DATA_LEN_PER;
static double recv_start, recv_end;
static double send_start, send_end;
static int recv_cnt;
static int wait_time = 10;
static int msgbox_is_full = 0;
static hal_sem_t finish = NULL;

struct rpmsg_demo_private {
	struct rpmsg_endpoint *ept;
	int is_unbound;
};

static struct rpmsg_demo_private demo = {
	.ept = NULL,
	.is_unbound = 0,
};

static double get_time(void)
{
	struct timeval tv;
	struct timezone tpz;

	gettimeofday(&tv,&tpz);
	return ( (double) tv.tv_sec * 1000 + (double) tv.tv_usec /1000);
}

static void msgbox_full_callback(void)
{
	int ret;
	char data[4] = {'f', 'u', 'l', 'l'};

	if (msgbox_is_full != 0)
		return;

	msgbox_is_full = 1;
	speed_debug("remote send too fast, adjuest...\r\n");
	if (demo.is_unbound)
		return;
	ret = openamp_rpmsg_send(demo.ept, (void *)data, 4);
	if (ret < 0) {
		speed_debug("Failed to send data(%d)\r\n", ret);
	}
}

static void msgbox_empty_callback(void)
{
	int ret;
	char data[4] = {'e', 'm', 'p', 't'};

	if (msgbox_is_full != 1)
		return;

	msgbox_is_full = 0;

	if (demo.is_unbound)
		return;

	ret = openamp_rpmsg_send(demo.ept, (void *)data, 4);
	if (ret < 0) {
		speed_debug("Failed to send data(%d)\r\n", ret);
	}
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{

	/* first data packet */
	if ((*(unsigned long *)data) == 0) {
		recv_cnt = 0;
		recv_start = get_time();
	}

	recv_cnt += len;

	if (recv_cnt >= data_len) {
		recv_end = get_time();
		recv_end -= recv_start;
		printf("Recv speed:%0.5lf m/s\n", recv_cnt / 1000 / recv_end);
		hal_sem_post(finish);
		recv_cnt = 0;
	}

	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	struct rpmsg_demo_private *demo_priv = ept->priv;

	printf("Remote endpoint is destroyed\n");
	demo_priv->is_unbound = 1;
}

static void print_help_msg(void)
{
	printf("\n");
	printf("USAGE:\n");
	printf("  rpmsg_demo [OPTIONS]\n");
	printf("OPTIONS:\n");
	printf("  -s MESSAGE  : send size(default 4M)\n");
	printf("e.g.\n");
	printf("  rpmsg_demo -s 100 : send size = 100 K\n");
	printf("\n");
}



static void cmd_rpmsg_demo_thread(void *arg)
{
	int ret;
	int i = 0;
	double t1, t2;
	double total_time = 0;
	long total_len = 0;
	double speed = 0;

	hal_msgbox_init();


	finish =  hal_sem_create(0);
	if (!finish) {
		printf("Failed to create sem\r\n");
		goto out;
	}

	if (openamp_init() != 0) {
		printf("Failed to init openamp framework\r\n");
		goto del_sem;
	}

	demo.ept = openamp_ept_open(name, 0, src_addr, dst_addr,
					&demo, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (demo.ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		goto out;
	}

	printf("Success to Create Endpoint\r\n");

	speed_debug("Start send data size = %d K\n", data_len / 1024);

	while (!demo.is_unbound && total_len < data_len) {
		*(long *)data_send = total_len;
		t1 = get_time();
		ret = openamp_rpmsg_send(demo.ept, (void *)data_send, DATA_LEN_PER);
		if(ret < 0) {
			printf("Failed to send data\r\n");
			goto free_ept;
		}
		t2 = get_time() - t1;

		total_time += t2;
		total_len += DATA_LEN_PER;
	}
	if (demo.is_unbound) {
		printf("Error:ept is closed by Remote\r\n");
		goto free_ept;
	}

	total_time  = (double)total_len / total_time;
	printf("Send Speed: %lf K/S(len:%ld t:%0.3lfms)\n", total_time,
					total_len, total_time);

	msgbox_set_full_callback(msgbox_full_callback);
	msgbox_set_empty_callback(msgbox_empty_callback);

	hal_sem_wait(finish);

	msgbox_reset_full_callback();
	msgbox_reset_empty_callback();

	if (recv_cnt > 0) {
		recv_end = get_time();
		recv_end -= recv_start;
		speed = recv_cnt / 1000 / recv_end;

		printf("Recv speed:%0.5lf m/s\n", recv_cnt / 1000 / recv_end);
	}
	printf("demo finish\r\n");
free_ept:
	openamp_ept_close(demo.ept);
del_sem:
	hal_sem_delete(finish);
out:
	return ;
}

static int cmd_rpmsg_demo(int argc, char *argv[])
{
	void *demo_thread;

	int ret = 0;
	int c;

	while((c = getopt(argc, argv, "hs:t:")) != -1) {
		switch(c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
			break;
		case 's':
			data_len = atoi(optarg) * 1024;
			break;
		case 't':
			wait_time = atoi(optarg);
			break;
		default:
			printf("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	demo_thread = kthread_create(cmd_rpmsg_demo_thread, NULL, "rpmsg_test", 2048, 10);
	if(demo_thread != NULL)
		kthread_start(demo_thread);
out:
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_demo, rpmsg_speed, rpmsg speed test);
