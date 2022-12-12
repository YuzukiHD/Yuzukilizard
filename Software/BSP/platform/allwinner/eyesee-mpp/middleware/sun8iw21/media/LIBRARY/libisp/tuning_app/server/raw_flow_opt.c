#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linux/videodev2.h"

#include "../log_handle.h"

#include "raw_flow_opt.h"


capture_format               *g_raw_queue = NULL;
int                          g_raw_queue_length = 0;
int                          g_raw_queue_head = 0;
int                          g_raw_queue_tail = 0;
pthread_mutex_t              g_raw_queue_locker;


int init_raw_flow(const capture_format *raw_fmt, int queue_length)
{
	capture_format *node = NULL;
	int i = 0, buffer_length = 0;

	if (!raw_fmt || queue_length < 0 ||
		raw_fmt->width < 0 || raw_fmt->height < 0) {
		LOG("%s: invalid params\n", __FUNCTION__);
		return ERR_RAW_FLOW_INVALID_PARAMS;
	}
	
	exit_raw_flow();

	if (raw_fmt->format == V4L2_PIX_FMT_SBGGR8 ||
		raw_fmt->format == V4L2_PIX_FMT_SGBRG8 ||
		raw_fmt->format == V4L2_PIX_FMT_SGRBG8 ||
		raw_fmt->format == V4L2_PIX_FMT_SRGGB8) {
		buffer_length = raw_fmt->width * raw_fmt->height;
	} else {
		buffer_length = raw_fmt->width * raw_fmt->height << 1;
	}
	g_raw_queue_length = queue_length;
	g_raw_queue = (capture_format *)malloc(sizeof(capture_format) * g_raw_queue_length);
	
	for (i = 0, node = g_raw_queue; i < g_raw_queue_length; i++, node++) {
		memset(node, 0, sizeof(capture_format));
		node->width = raw_fmt->width;
		node->height = raw_fmt->height;
		node->format = raw_fmt->format;
		node->planes_count = raw_fmt->planes_count;
		node->buffer = (unsigned char *)malloc(buffer_length);
	}

	g_raw_queue_head = 0;
	g_raw_queue_tail = 0;

	pthread_mutex_init(&g_raw_queue_locker, NULL);
	
	return ERR_RAW_FLOW_NONE;
}

int exit_raw_flow()
{
	if (g_raw_queue_length > 0) {
		pthread_mutex_lock(&g_raw_queue_locker);
		free(g_raw_queue);
		g_raw_queue = NULL;
		g_raw_queue_length = 0;
		g_raw_queue_head = 0;
		g_raw_queue_tail = 0;
		pthread_mutex_unlock(&g_raw_queue_locker);
		pthread_mutex_destroy(&g_raw_queue_locker);
		LOG("%s: queue destroyed\n", __FUNCTION__);
	}
	return ERR_RAW_FLOW_NONE;
}

int queue_raw_flow(const capture_format *node)
{
	int head = 0;
	capture_format *head_node = NULL;

#define SAVE_RAW_FLOW  1
#if SAVE_RAW_FLOW
	int i = 0;
	char save_file_name[256];
	FILE *fp = NULL;
#endif

	if (!node || !node->buffer) {
		LOG("%s: invalid params\n", __FUNCTION__);
		return ERR_RAW_FLOW_INVALID_PARAMS;
	}
	
	if (g_raw_queue_length > 0) {
		pthread_mutex_lock(&g_raw_queue_locker);
		head_node = g_raw_queue + g_raw_queue_head;
		if (head_node->width != node->width ||
			head_node->height != node->height ||
			head_node->format != node->format) {
			LOG("%s: format not matched - fmt:%d,%dx%d <> fmt:%d,%dx%d\n", __FUNCTION__,
				head_node->format, head_node->width, head_node->height,
				node->format, node->width, node->height);
			pthread_mutex_unlock(&g_raw_queue_locker);
			return ERR_RAW_FLOW_FORMAT;
		}
		head = (g_raw_queue_head + 1) % g_raw_queue_length;
		if (head == g_raw_queue_tail) {
			LOG("%s: queue is full(head:%d, tail:%d)\n", __FUNCTION__,
				g_raw_queue_head, g_raw_queue_tail);

#if SAVE_RAW_FLOW
            i = sprintf(save_file_name, "/mnt/extsd/raw_");
			get_sys_time(save_file_name + i, "%04d-%02d-%02d_%02d-%02d-%02d");
			sprintf(save_file_name + i + 19, ".bin");
			fp = fopen(save_file_name, "wb");
			if (fp) {
				LOG("%s: ready to write to file: %s\n", __FUNCTION__, save_file_name);
				while (g_raw_queue_tail != g_raw_queue_head) {
					head_node = g_raw_queue + g_raw_queue_tail;
					fwrite(head_node->buffer, head_node->length, 1, fp);
					LOG("%s: tail %d write done\n", __FUNCTION__, g_raw_queue_tail);
					g_raw_queue_tail = (g_raw_queue_tail + 1) % g_raw_queue_length;
				}
				fclose(fp);
				fp = NULL;
				LOG("%s: all write done\n", __FUNCTION__);
			}
#endif
			
			pthread_mutex_unlock(&g_raw_queue_locker);
			return ERR_RAW_FLOW_QUEUE_FULL;
		}
		// copy data
		//head_node->width = node->width;
		//head_node->height = node->height;
		//head_node->format = node->format;
		head_node->planes_count = node->planes_count;
		head_node->length = node->length;
		head_node->width_stride[0] = node->width_stride[0];
		head_node->width_stride[1] = node->width_stride[1];
		head_node->width_stride[2] = node->width_stride[2];
		memcpy(head_node->buffer, node->buffer, node->length);
		
		LOG("%s: queue done(head:%d->%d, tail:%d)\n", __FUNCTION__,
			g_raw_queue_head, head, g_raw_queue_tail);
		g_raw_queue_head = head;
		pthread_mutex_unlock(&g_raw_queue_locker);
		return ERR_RAW_FLOW_NONE;		
	}
	
	LOG("%s: queue not init\n", __FUNCTION__);
	return ERR_RAW_FLOW_NOT_INIT;
}

int dequeue_raw_flow(capture_format *node)
{
	int tail = 0;
	capture_format *tail_node = NULL;
	
	if (g_raw_queue_length > 0) {
		LOG("%s: %d\n", __FUNCTION__, __LINE__);
		pthread_mutex_lock(&g_raw_queue_locker);
		LOG("%s: %d\n", __FUNCTION__, __LINE__);
		tail_node = g_raw_queue + g_raw_queue_tail;
		if (g_raw_queue_tail == g_raw_queue_head) {
			LOG("%s: queue is empty(head:%d, tail:%d)\n", __FUNCTION__,
				g_raw_queue_head, g_raw_queue_tail);
			pthread_mutex_unlock(&g_raw_queue_locker);
			return ERR_RAW_FLOW_QUEUE_EMPTY;
		}
		
		if (node) {
			node->width = tail_node->width;
			node->height = tail_node->height;
			node->format = tail_node->format;
			node->planes_count = tail_node->planes_count;
			node->width_stride[0] = tail_node->width_stride[0];
			node->width_stride[1] = tail_node->width_stride[1];
			node->width_stride[2] = tail_node->width_stride[2];
			node->length = tail_node->length;
			memcpy(node->buffer, tail_node->buffer, tail_node->length);
		}
		
		tail = (g_raw_queue_tail + 1) % g_raw_queue_length;
		LOG("%s: dequeue done(head:%d, tail:%d->%d)\n", __FUNCTION__,
			g_raw_queue_head, g_raw_queue_tail, tail);
		g_raw_queue_tail = tail;
		pthread_mutex_unlock(&g_raw_queue_locker);
		return ERR_RAW_FLOW_NONE;		
	}

	LOG("%s: queue not init\n", __FUNCTION__);
	return ERR_RAW_FLOW_NOT_INIT;
}




