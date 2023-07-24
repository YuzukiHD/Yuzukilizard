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

#define RPMSG_SERVICE_NAME "sunxi,sun8iw21p1-e907"
#define SEM_RECEIVED_WAIT_MS 100

static const char *name = RPMSG_SERVICE_NAME;
static uint32_t src_addr = RPMSG_ADDR_ANY;
static uint32_t dst_addr = RPMSG_ADDR_ANY;
static int do_send = 0;
static char data_send[256];
static int do_recv = 0;
static int timeout_sec = 10;

struct rpmsg_demo_private {
	hal_sem_t processing;

	int rpmsg_id;
	struct rpmsg_endpoint *ept;
	int is_unbound;
	int is_ready;
};

static double get_time(void)
{
	struct timeval tv;
	struct timezone tpz;

	gettimeofday(&tv,&tpz);
	return ( (double) tv.tv_sec * 1000 + (double) tv.tv_usec /1000);
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct rpmsg_demo_private *demo = priv;
	char *d = data;
	int ret, i;

	if (!do_recv)
		return 0;

	ret =  hal_sem_wait(demo->processing);
	if (ret != 0) {
		printf("Previous data is still processing. Discard new data\n");
		ret = -1;
		goto out;
	}

	ret = hal_sem_post(demo->processing);
	if (ret != 0) {
		printf("Failed to give processing\n");
		ret = -1;
		goto out;
	}

	d[len] = '\0';
	printf("Received data (len: %d):%s\n", len, d);

	ret = 0;
out:
	return ret;
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
	printf("  -s MESSAGE  : send MESSAGE\n");
	printf("  -r          : receive messages\n");
	printf("  -t TIMEOUT  : exit in TIMEOUT seconds when receive\n");
	printf("  -I ID       : specify rpmsg device ID (default: 0)\n");
	printf("  -N NAME     : specify endpoint service name"
			" (default is \"%s\")\n", RPMSG_SERVICE_NAME);
	printf("  -S SRC_ADDR : specify source address of endpoint (hexadecimal)"
			" (default is RPMSG_ADDR_ANY: 0x%x)\n", RPMSG_ADDR_ANY);
	printf("  -D DST_ADDR : specify destination address of endpoint (hexadecimal)"
			" (default is RPMSG_ADDR_ANY: 0x%x)\n", RPMSG_ADDR_ANY);
	printf("e.g.\n");
	printf("  rpmsg_demo -s \"hello\" : send string \"hello\"\n");
	printf("  rpmsg_demo -r           : receive messages (default in 10 seconds)\n");
	printf("  rpmsg_demo -r -t 20     : receive messages in 20 seconds\n");
	printf("\n");
}

static struct rpmsg_demo_private demo = {
	.processing = NULL,

	.rpmsg_id = 0,
	.ept = NULL,
	.is_unbound = 0,
};

static void cmd_rpmsg_demo_thread(void *arg)
{
	int ret = 0;
	double t1, t2;

	printf("Wait Remoteproc readdy...\r\n");
	if (openamp_init() != 0) {
		printf("Failed to init openamp framework\r\n");
		goto free_sem;
	}

	/* Prepare semphore */
	if(do_recv) {
		demo.processing = hal_sem_create(1);
		if(!demo.processing) {
			printf("Failed to Create processing\r\n");
			goto out;
		}
	}

	printf("Creating Endpoint...\r\n");
	demo.ept = openamp_ept_open(name, demo.rpmsg_id, src_addr, dst_addr,
					&demo, rpmsg_ept_callback, rpmsg_unbind_callback);
	if(demo.ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		goto free_sem;
	}

	printf("Success to Create Endpoint\r\n");
	printf("rpmsg_demo will exit in %d seconds\n", timeout_sec);

	if(do_recv) {
		t1 = get_time();
		t1 += timeout_sec * 1000;
		while(!demo.is_unbound) {

			ret = openamp_rpmsg_send(demo.ept, (void *)data_send, strlen(data_send));
			if(ret < 0) {
				printf("Failed to send data\r\n");
				goto free_ept;
			}

			kthread_msleep(500);
			t2 = get_time();

			if(t2 >= t1)
				break;
		}
	}

	printf("demo finish\r\n");

free_ept:
	openamp_ept_close(demo.ept);
free_sem:
	if (demo.processing != NULL)
		hal_sem_delete(demo.processing);
out:
	return ;
}

static int cmd_rpmsg_demo(int argc, char *argv[])
{
	void *demo_thread;

	int ret = 0;
	int c;

	strncpy(data_send, "hello", sizeof(data_send));
	while((c = getopt(argc, argv, "hs:rt:I:N:S:D:")) != -1) {
		switch(c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
		case 's':
			do_send = 1;
			strncpy(data_send, optarg, sizeof(data_send));
			break;
		case 'r':
			do_recv = 1;
			break;
		case 't':
			timeout_sec = atoi(optarg);
			break;
		case 'I':
			demo.rpmsg_id = atoi(optarg);
			break;
		case 'N':
			name = optarg;
			break;
		case 'S':
			src_addr = strtoul(optarg, NULL, 16);
			break;
		case 'D':
			dst_addr = strtoul(optarg, NULL, 16);
			break;
		default:
			printf("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	demo_thread = kthread_create(cmd_rpmsg_demo_thread, NULL, "rpmsg_demo", HAL_THREAD_STACK_SIZE, HAL_THREAD_PRIORITY);
	if(demo_thread != NULL)
		kthread_start(demo_thread);

out:
	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpmsg_demo, rpmsg_demo, rpmsg demo);
