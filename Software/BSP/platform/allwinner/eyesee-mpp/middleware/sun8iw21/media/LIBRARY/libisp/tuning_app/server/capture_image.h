#ifndef _CAPTURE_IMAGE_H_V100_
#define _CAPTURE_IMAGE_H_V100_

#ifdef ANDROID_VENCODE
#include "../../isp_vencode/include/ispSimpleCode.h"
#endif

typedef enum _capture_error_e {
	CAP_ERR_NONE               = 0x00,
	CAP_ERR_MPI_INIT,
	CAP_ERR_MPI_EXIT,
	CAP_ERR_CH_INIT,
	CAP_ERR_CH_EXIT,
	CAP_ERR_CH_SET_FMT,
	CAP_ERR_CH_GET_FMT,
	CAP_ERR_CH_ENABLE,
	CAP_ERR_CH_DISABLE,
	CAP_ERR_GET_FRAME,
	CAP_ERR_RELEASE_FRAME,
	CAP_ERR_INVALID_PARAMS,
	CAP_ERR_NOT_READY,
	CAP_ERR_NOT_SET_INPUT,
	CAP_ERR_START_RAW_FLOW,
	CAP_ERR_STOP_RAW_FLOW,
	CAP_ERR_RAW_FLOW_NOT_RUN,
	CAP_ERR_GET_RAW_FLOW,
	CAP_ERR_VENCODE_PPSSPS,
	CAP_ERR_VENCODE_STREAM,
	CAP_ERR_VENCODE_FREEBUFFER,
} capture_error;

typedef struct _capture_format_s {
	int                      channel; // for input
	unsigned char            *buffer; // for input and output
	int                      length;  // for output
	int                      width;   // for intpu and output
	int                      height;  // for input and output
	int                      fps;     // for input
	int                      wdr;     // for input
	int                      format;  // for input
	int                      planes_count; // planes count
	int			 framecount;//frame count
	int                      width_stride[3]; // width stride for each plane
	int						index; //rear:0 front:1
} capture_format;

typedef struct _sensor_input_s {
	int                      isp;
	int                      channel;
	int                      width;
	int                      height;
	int                      fps;
	int                      wdr;
	int                      format;
	int						index; //rear:0 front:1
} sensor_input;

typedef struct _region_s
{
	int                                   left;
	int                                   top;
	int                                   right;
	int                                   bottom;
} SRegion, *pSRegion;

/*
 * init
 * returns capture_error code
 */
int init_capture_module();
/*
 * exit
 * returns capture_error code
 */
int exit_capture_module();
/*
 * get vich channel status
 * bit[i] - vich i status, 0 - not open, 1 - opened
 * returns all vich channels status
 */
int get_vich_status();
/*
 * set sensor input
 * returns capture_error code
 */
int set_sensor_input(const sensor_input *sensor_in);
/*
 * get capture buffer
 * cap_fmt->buffer should alloc in advance
 * returns capture_error code
 */
int get_capture_buffer(capture_format *cap_fmt);

/*
 * get capture blockinfo
 * cap_fmt->buffer should alloc in advance
 * returns capture_error code
 */
int get_capture_blockinfo(capture_format *cap_fmt, int GrayBlocksFlag, SRegion *region);

#ifdef ANDROID_VENCODE
/*
 * update vencode config
 * return capture_error code
 */
int set_vencode_config(capture_format *cap_fmt, encode_param_t *encode_param, int type);
/*
 * get capture-video encoder buffer
 * cap_fmt->buffer should alloc in advance
 * returns capture_error code
 */
int get_capture_vencode_buffer(capture_format *cap_fmt, encode_param_t *encode_param, int type);
#endif

/*
 * get capture buffer to transfer
 * cap_fmt->buffer should alloc in advance
 * returns capture_error code
 */
int get_capture_buffer_transfer(capture_format *cap_fmt);
/*
 * start raw flow
 * returns capture_error code
 */
 int start_raw_flow(capture_format *cap_fmt);
/*
 * stop raw flow
 * returns capture_error code
 */
int stop_raw_flow(int channel);
/*
 * get one raw flow data
 * cap_fmt->buffer should alloc in advance
 * returns capture_error code
 */
int get_raw_flow_frame(capture_format *cap_fmt);
/*
 * save raw data to local
 */
void save_raw_flow(const char *file_name);


#endif /* _CAPTURE_IMAGE_H_V100_ */
