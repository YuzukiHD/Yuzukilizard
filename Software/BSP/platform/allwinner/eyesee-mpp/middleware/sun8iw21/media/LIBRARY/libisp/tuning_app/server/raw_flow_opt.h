#ifndef _RAW_FLOW_OPT_H_V100_
#define _RAW_FLOW_OPT_H_V100_

#include "capture_image.h"

typedef enum _raw_flow_err_e {
	ERR_RAW_FLOW_NONE                  = 0,
	ERR_RAW_FLOW_INVALID_PARAMS,
	ERR_RAW_FLOW_EXIT,
	ERR_RAW_FLOW_NOT_INIT,
	ERR_RAW_FLOW_FORMAT,
	ERR_RAW_FLOW_QUEUE_FULL,
	ERR_RAW_FLOW_QUEUE_EMPTY,
} eRawFlowErr;

/*
 * init raw flow queue
 * the size of memory to alloc is: sizeof(capture_format) * queue_length
 * raw_fmt: queue node format, raw_fmt->buffer is not used
 * queue_length: queue length
 * returns eRawFlowErr code
 */
int init_raw_flow(const capture_format *raw_fmt, int queue_length);
/*
 * exit raw flow
 * free queue data
 * returns eRawFlowErr code
 */
int exit_raw_flow();
/*
 * queue operation
 * queue one node to the head
 * node: raw data, format must be matched and data buffer is ready
 * returns eRawFlowErr code
 */
int queue_raw_flow(const capture_format *node);
/*
 * dequeue operation
 * dequeue one node from the tail
 * node: raw data, data buffer should alloc in advance
 * returns eRawFlowErr code
 */
int dequeue_raw_flow(capture_format *node);



#endif /* _RAW_FLOW_OPT_H_V100_ */

