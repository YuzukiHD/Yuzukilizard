/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
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
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <init.h>
#include <aw_list.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <rpbuf.h>

#define RPBUF_BUFFER_NAME_DEFAULT "rpbuf_demo"
#define RPBUF_BUFFER_LENGTH_DEFAULT 32

#define RPBUF_DEMO_LOG(controller_id, buf_name, buf_len, fmt, args...) \
	printf("[%d|%s|%d] " fmt, controller_id, buf_name, buf_len, ##args)

struct rpbuf_demo_buffer_entry {
	int controller_id;
	struct rpbuf_buffer *buffer;
	struct list_head list;
};

LIST_HEAD(rpbuf_demo_buffers);
static hal_mutex_t rpbuf_demo_buffers_mutex;

static int rpbuf_demo_mutex_create(void)
{
	rpbuf_demo_buffers_mutex = hal_mutex_create();
	return 0;
}
core_initcall(rpbuf_demo_mutex_create);

static void print_help_msg(void)
{
	printf("\n");
	printf("USAGE:\n");
	printf("  rpbuf_demo [OPTIONS]\n");
	printf("OPTIONS:\n");
	printf("  -h          : print help message\n");
	printf("  -c          : create buffer\n");
	printf("  -d          : destory buffer\n");
	printf("  -s STRING   : copy STRING to buffer and send to remote\n");
	printf("  -l          : list created buffers\n");
	printf("  -I ID       : specify controller ID (default: 0)\n");
	printf("  -N NAME     : specify buffer name (default: \"%s\")\n",
			RPBUF_BUFFER_NAME_DEFAULT);
	printf("  -L LENGTH   : specify buffer length (default: %d bytes)\n",
			RPBUF_BUFFER_LENGTH_DEFAULT);
	printf("  -O OFFSET   : specify offset in buffer to store data (default: 0)\n");
	printf("\n");
	printf("e.g.\n");
	printf("  First, create a buffer (its name and length should match "
			"that of remote rpbuf buffer):\n");
	printf("      rpbuf_buffer -N \"xxx\" -L LENGTH -c\n");
	printf("  Then if remote sends data to it, the buffer callback will be called.\n");
	printf("\n");
	printf("  We can send data (e.g. \"hello\") to remote:\n");
	printf("      rpbuf_demo -N \"xxx\" -s \"hello\"\n");
	printf("\n");
	printf("  If this buffer is no longer in use, destroy it:\n");
	printf("      rpbuf_demo -N \"xxx\" -d\n");
	printf("\n");
}

static struct rpbuf_demo_buffer_entry *find_buffer_entry(const char *name)
{
	struct rpbuf_demo_buffer_entry *buf_entry;

	list_for_each_entry(buf_entry, &rpbuf_demo_buffers, list) {
		if (0 == strcmp(rpbuf_buffer_name(buf_entry->buffer), name))
			return buf_entry;
	}
	return NULL;
}

static void rpbuf_demo_buffer_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	printf("buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}

static int rpbuf_demo_buffer_rx_cb(struct rpbuf_buffer *buffer,
		void *data, int data_len, void *priv)
{
	int i;

	printf("buffer \"%s\" received data (addr: %p, offset: %d, len: %d):\n",
			rpbuf_buffer_name(buffer), rpbuf_buffer_va(buffer),
			data - rpbuf_buffer_va(buffer), data_len);
	for (i = 0; i < data_len; ++i)
		printf(" 0x%x,", *((uint8_t *)(data) + i));
	printf("\n");

	return 0;
}

static int rpbuf_demo_buffer_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	printf("buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));

	return 0;
}

static const struct rpbuf_buffer_cbs rpbuf_demo_cbs = {
	.available_cb = rpbuf_demo_buffer_available_cb,
	.rx_cb = rpbuf_demo_buffer_rx_cb,
	.destroyed_cb = rpbuf_demo_buffer_destroyed_cb,
};

static int rpbuf_demo_create(int controller_id, const char *name, int len)
{
	int ret;
	struct rpbuf_demo_buffer_entry *buf_entry = NULL;
	struct rpbuf_controller *controller = NULL;
	struct rpbuf_buffer *buffer = NULL;

	hal_mutex_lock(rpbuf_demo_buffers_mutex);
	buf_entry = find_buffer_entry(name);
	if (buf_entry) {
		printf("Buffer named \"%s\" already exists\n", name);
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		ret = -EINVAL;
		goto err_out;
	}
	hal_mutex_unlock(rpbuf_demo_buffers_mutex);

	buf_entry = hal_malloc(sizeof(struct rpbuf_demo_buffer_entry));
	if (!buf_entry) {
		RPBUF_DEMO_LOG(controller_id, name, len,
				"Failed to allocate memory for buffer entry\n");
		ret = -ENOMEM;
		goto err_out;
	}
	buf_entry->controller_id = controller_id;

	controller = rpbuf_get_controller_by_id(controller_id);
	if (!controller) {
		RPBUF_DEMO_LOG(controller_id, name, len,
				"Failed to get controller%d, controller_id\n", controller_id);
		ret = -ENOENT;
		goto err_free_buf_entry;
	}

	buffer = rpbuf_alloc_buffer(controller, name, len, NULL, &rpbuf_demo_cbs, NULL);
	if (!buffer) {
		RPBUF_DEMO_LOG(controller_id, name, len, "rpbuf_alloc_buffer failed\n");
		ret = -ENOENT;
		goto err_free_buf_entry;
	}
	buf_entry->buffer = buffer;

	hal_mutex_lock(rpbuf_demo_buffers_mutex);
	list_add_tail(&buf_entry->list, &rpbuf_demo_buffers);
	hal_mutex_unlock(rpbuf_demo_buffers_mutex);

	return 0;

err_free_buf_entry:
	hal_free(buf_entry);
err_out:
	return ret;
}

static int rpbuf_demo_destroy(const char *name)
{
	int ret;
	struct rpbuf_demo_buffer_entry *buf_entry;
	struct rpbuf_buffer *buffer;

	hal_mutex_lock(rpbuf_demo_buffers_mutex);

	buf_entry = find_buffer_entry(name);
	if (!buf_entry) {
		printf("Buffer named \"%s\" not found\n", name);
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		ret = -EINVAL;
		goto err_out;
	}
	buffer = buf_entry->buffer;

	ret = rpbuf_free_buffer(buffer);
	if (ret < 0) {
		RPBUF_DEMO_LOG(buf_entry->controller_id,
				rpbuf_buffer_name(buffer),
				rpbuf_buffer_len(buffer),
				"rpbuf_free_buffer failed\n");
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		goto err_out;
	}

	list_del(&buf_entry->list);
	hal_free(buf_entry);

	hal_mutex_unlock(rpbuf_demo_buffers_mutex);

	return 0;

err_out:
	return ret;
}

static int rpbuf_demo_transmit(const char *name, void *data, int data_len, int offset)
{
	int ret;
	struct rpbuf_demo_buffer_entry *buf_entry;
	struct rpbuf_buffer *buffer;

	hal_mutex_lock(rpbuf_demo_buffers_mutex);
	buf_entry = find_buffer_entry(name);
	if (!buf_entry) {
		printf("Buffer named \"%s\" not found\n", name);
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		ret = -EINVAL;
		goto err_out;
	}
	buffer = buf_entry->buffer;

	/*
	 * Before putting data to buffer or sending buffer to remote, we should
	 * ensure that the buffer is available.
	 */
	if (!rpbuf_buffer_is_available(buffer)) {
		RPBUF_DEMO_LOG(buf_entry->controller_id,
				rpbuf_buffer_name(buffer),
				rpbuf_buffer_len(buffer),
				"buffer not available\n");
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		ret = -EACCES;
		goto err_out;
	}

	if (data_len > rpbuf_buffer_len(buffer)) {
		RPBUF_DEMO_LOG(buf_entry->controller_id,
				rpbuf_buffer_name(buffer),
				rpbuf_buffer_len(buffer),
				"data length too long\n");
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		ret = -EACCES;
		goto err_out;
	}
	memcpy(rpbuf_buffer_va(buffer) + offset, data, data_len);

	ret = rpbuf_transmit_buffer(buffer, offset, data_len);
	if (ret < 0) {
		RPBUF_DEMO_LOG(buf_entry->controller_id,
				rpbuf_buffer_name(buffer),
				rpbuf_buffer_len(buffer),
				"rpbuf_transmit_buffer failed\n");
		hal_mutex_unlock(rpbuf_demo_buffers_mutex);
		goto err_out;
	}

	hal_mutex_unlock(rpbuf_demo_buffers_mutex);

	return 0;

err_out:
	return ret;
}

static void rpbuf_demo_list_buffers(void)
{
	struct rpbuf_demo_buffer_entry *buf_entry;
	struct rpbuf_buffer *buffer;

	hal_mutex_lock(rpbuf_demo_buffers_mutex);

	printf("\nCreated buffers:\n");
	list_for_each_entry(buf_entry, &rpbuf_demo_buffers, list) {
		buffer = buf_entry->buffer;
		printf("  [%d|%s] id: %d, va: %p, pa: 0x%x, len: %d\n",
				buf_entry->controller_id,
				rpbuf_buffer_name(buffer), rpbuf_buffer_id(buffer),
				rpbuf_buffer_va(buffer), rpbuf_buffer_pa(buffer),
				rpbuf_buffer_len(buffer));
	}
	printf("\n");

	hal_mutex_unlock(rpbuf_demo_buffers_mutex);
}

static int cmd_rpbuf_demo(int argc, char *argv[])
{
	int ret = 0;
	int c;

	int do_create = 0;
	int do_destroy = 0;
	int do_send = 0;
	int do_list_buffers = 0;

	int controller_id = 0;
	const char *name = RPBUF_BUFFER_NAME_DEFAULT;
	long len = RPBUF_BUFFER_LENGTH_DEFAULT;
	int offset = 0;

	const char *data_send = NULL;

	if (argc <= 1) {
		print_help_msg();
		ret = -1;
		goto out;
	}

	while ((c = getopt(argc, argv, "hcds:lI:N:L:O:")) != -1) {
		switch (c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
		case 'c':
			do_create = 1;
			break;
		case 'd':
			do_destroy = 1;
			break;
		case 's':
			do_send = 1;
			data_send = optarg;
			break;
		case 'l':
			do_list_buffers = 1;
			break;
		case 'I':
			controller_id = atoi(optarg);
			break;
		case 'N':
			name = optarg;
			break;
		case 'L':
			len = strtol(optarg, NULL, 0);
			if (len == -ERANGE) {
				printf("Invalid length arg.\r\n");
				goto out;
			}
			break;
		case 'O':
			offset = atoi(optarg);
			break;
		default:
			printf("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	if (do_create) {
		ret = rpbuf_demo_create(controller_id, name, len);
		if (ret < 0) {
			printf("rpbuf_demo_create failed\n");
			goto out;
		}
	}

	if (do_send) {
		ret = rpbuf_demo_transmit(name, (void *)data_send, strlen(data_send), offset);
		if (ret < 0) {
			printf("rpbuf_demo_transmit (with data) failed\n");
			goto out;
		}
	}

	if (do_destroy) {
		ret = rpbuf_demo_destroy(name);
		if (ret < 0) {
			printf("rpbuf_demo_destroy failed\n");
			goto out;
		}
	}

	if (do_list_buffers) {
		rpbuf_demo_list_buffers();
	}

out:
	return ret;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rpbuf_demo, rpbuf_demo, rpbuf demo);
