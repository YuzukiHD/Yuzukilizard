/*
 * my_sws.c
 *
 *  Created on: 2014-8-17
 *      Author: liu
 */
#include "my_sws.h"

int swscale_init(swscale_s* args) {
	/* create scaling context */
	int ret;
	args->sws_ctx = sws_getContext(args->src_w, args->src_h, args->src_pix_fmt,
			args->dst_w, args->dst_h, args->dst_pix_fmt,
			SWS_BILINEAR, NULL, NULL, NULL);
	if (!args->sws_ctx) {
		fprintf(stderr, "Impossible to create scale context for the conversion "
				"fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
				av_get_pix_fmt_name(args->src_pix_fmt), args->src_w,
				args->src_h, av_get_pix_fmt_name(args->dst_pix_fmt),
				args->dst_w, args->dst_h);
		ret = AVERROR(EINVAL);
		return -1;
	}
	/* allocate source and destination image buffers */
	if ((ret = av_image_alloc(args->src_data, args->src_linesize, args->src_w,
			args->src_h, args->src_pix_fmt, 16)) < 0) {
		fprintf(stderr, "Could not allocate source image\n");
		return -1;
	}
	/* buffer is going to be written to rawvideo file, no alignment */
	if ((ret = av_image_alloc(args->dst_data, args->dst_linesize, args->dst_w,
			args->dst_h, args->dst_pix_fmt, 1)) < 0) {
		fprintf(stderr, "Could not allocate destination image\n");
		return -1;
	}
	args->dst_bufsize = ret;

	return ret;
}

int swscale_convert(swscale_s* args) {
	/* generate synthetic video */
//    fill_yuv_image(args->src_data, args->src_linesize, args->src_w, args->src_h, i);
	/* convert to destination format */
	return sws_scale(args->sws_ctx, (const uint8_t * const *) (args->src_data),
			args->src_linesize, 0, args->src_h, args->dst_data,
			args->dst_linesize);
}

int swscale_write_YUYV(swscale_s* args, unsigned char* data, int length) {
	memcpy(args->src_data[0], data, length);
	printf("copy data to src_data%d\n", length);

	return 0;
}

int swscale_read_rgb(swscale_s* args, unsigned char *data) {
	memcpy(data, args->dst_data[0], args->dst_bufsize);
	printf("copy rgb over!\n size: %d", args->dst_bufsize);

	return 0;
}

void swscale_free(swscale_s* args) {
	av_freep(&args->src_data[0]);
	av_freep(&args->dst_data[0]);
	sws_freeContext(args->sws_ctx);
}

//int sws_example(int argc, char **argv) {
//	swscale_s tmp, *sws;
//	sws = &tmp;
//	sws->dst_w = 320;
//	sws->dst_h = 240;
//	sws->src_w = 320;
//	sws->src_h = 240;
//	sws->dst_pix_fmt = AV_PIX_FMT_RGB24;
//	sws->src_pix_fmt = AV_PIX_FMT_YUYV422;
//
//	unsigned char test[320 * 240 * 3];
//	Camera *camera_s;
//
//	camera_s = calloc_camera();
//	camera_s->width = 320;
//	camera_s->height = 240;
//	camera_s->type = V4L2_PIX_FMT_YUYV;// in v4l2 drive
//
//	captureInit(camera_s);
//	swscale_init(sws);
//
//	int size;
//	while(read_frame(camera_s, test, &size));
//	swscale_write_YUYV(sws, test, camera_s->buf->length);
//	swscale_convert(sws);
//	swscale_read_rgb(sws, test);
//	write_data_to_file(test, sws->dst_bufsize, "test");
//
//	stop_capturing(camera_s);
//	free_camera(camera_s);
//	swscale_free(sws);
//
//	return 0;
//}



