/*
 * my_sws.h
 *
 *  Created on: 2014-8-17
 *      Author: liu
 */

#ifndef MY_SWS_H_
#define MY_SWS_H_

#include <linux/videodev2.h>

#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>

typedef struct _swscale {
	int src_w, src_h;
	int dst_w, dst_h;
	uint8_t *src_data[4], *dst_data[4];
	int src_linesize[4], dst_linesize[4];
	int dst_bufsize;
	struct SwsContext *sws_ctx;
	enum AVPixelFormat src_pix_fmt, dst_pix_fmt;
} swscale_s;

#endif /* MY_SWS_H_ */
