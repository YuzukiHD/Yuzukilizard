/*
 * encode.c
 *
 *  Created on: 2014-8-16
 *      Author: liu
 */

#include <signal.h>
#include "camer.h"
#include "encode.h"
#include "libavformat/avformat.h"

//static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base,
//		AVStream *st, AVPacket *pkt) {
//	/* rescale output packet timestamp values from codec to stream timebase */
//	pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base,
//			AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
//	pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base,
//			AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
//	pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
//	pkt->stream_index = st->index;
//	/* Write the compressed frame to the media file. */
////    log_packet(fmt_ctx, pkt);
//	return av_interleaved_write_frame(fmt_ctx, pkt);
//}
int flush_encoder(Encode* encode, unsigned int stream_index)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;

    while (1) {
        printf("Flushing stream #%u encoder\n", stream_index);
        //ret = encode_write_frame(NULL, stream_index, &got_frame);
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2(encode->pCodecCtx, &enc_pkt, NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0) break;
        if (!got_frame) {
            printf("缓存数据，编码成功1帧！\n");
            ret = 0;
            break;
        }
    }
    return ret;
}

int ffmpeg_init(Encode* encode)
{
    int ret = 0;

    avcodec_register_all();
    encode->pCodec = avcodec_find_encoder(AV_CODEC_ID_H265);
    if (!encode->pCodec) {
        printf("没有找到合适的编码器！\n");
        exit(1);
    }
    encode->pCodecCtx = avcodec_alloc_context3(encode->pCodec);

    encode->pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    encode->pCodecCtx->width = encode->in_w;
    encode->pCodecCtx->height = encode->in_h;
    encode->pCodecCtx->time_base.num = 1;
    encode->pCodecCtx->time_base.den = 10;
    printf("encode %dx%d, and the time_base num: %d den: %d\n", encode->in_w, encode->in_h,
           encode->pCodecCtx->time_base.num, encode->pCodecCtx->time_base.den);

    encode->pCodecCtx->bit_rate = 200000;
    encode->pCodecCtx->gop_size = 20;
    encode->pCodecCtx->max_b_frames = 0;
    //H264
    //encode->pCodecCtx->me_range = 16;
    //encode->pCodecCtx->max_qdiff = 4;
    //	encode->pCodecCtx->qmin = 8;
    //	encode->pCodecCtx->qmax = 31;
    //encode->pCodecCtx->qcompress = 0.6;

    //	av_opt_set(encode->pCodecCtx->priv_data, "preset", "slow", 0);
    av_opt_set(encode->pCodecCtx->priv_data, "profile", "baseline", 0);
    av_opt_set(encode->pCodecCtx->priv_data, "preset", "superfast", 0);
    av_opt_set(encode->pCodecCtx->priv_data, "tune", "zerolatency", 0);

    //swscale init
    enum AVPixelFormat sws_fmt = AV_PIX_FMT_YUYV422;
    encode->sws_ctx =
      sws_getContext(encode->pCodecCtx->width, encode->pCodecCtx->height, sws_fmt, encode->pCodecCtx->width,
                     encode->pCodecCtx->height, encode->pCodecCtx->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
    if ((encode->src_size = av_image_alloc(encode->src_data, encode->src_linesize, encode->pCodecCtx->width,
                                           encode->pCodecCtx->height, sws_fmt, 1)) < 0) {
        fprintf(stderr, "Could not allocate source image\n");
        return -1;
    }

    printf("========== src_size: %d\n", encode->src_size);
    printf("========== src_height: %d\n", encode->pCodecCtx->height);
    printf("========== src_width: %d\n", encode->pCodecCtx->width);
    printf("========== src_linesize[0]: %d\n", encode->src_linesize[0]);
    printf("========== src_linesize[1]: %d\n", encode->src_linesize[1]);
    printf("========== src_linesize[2]: %d\n", encode->src_linesize[2]);
    printf("========== src_linesize[3]: %d\n", encode->src_linesize[3]);
    encode->bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    if (!encode->bsfc) {
        printf("bitstream_filter_init failed!\n");
        av_bitstream_filter_close(encode->bsfc);
        encode->bsfc = NULL;
    }

    if (avcodec_open2(encode->pCodecCtx, encode->pCodec, NULL) < 0) {
        printf("编码器打开失败！\n");
        return -1;
    }
    encode->picture = av_frame_alloc();
    encode->picture->format = encode->pCodecCtx->pix_fmt;
    encode->picture->width = encode->pCodecCtx->width;
    encode->picture->height = encode->pCodecCtx->height;

    ret = av_image_alloc(encode->picture->data, encode->picture->linesize, encode->pCodecCtx->width,
                         encode->pCodecCtx->height, encode->pCodecCtx->pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "could not allow raw picture buffer\n");
        exit(1);
    }

    return 0;
}

int ffmpeg_encode_end(Encode* encode)
{
    //Flush Encoder
    int ret = flush_encoder(encode, 0);
    if (ret < 0) {
        printf("Flushing encoder failed\n");
        return -1;
    }

    //编码器相关资源清理

    av_bitstream_filter_close(encode->bsfc);
    avcodec_close(encode->pCodecCtx);
    avcodec_free_context(&(encode->pCodecCtx));
    av_free(encode->picture);
    //	avformat_free_context(encode->pFormatCtx);
    //swscale格式转换相关资源释放
    av_freep(&encode->src_data[0]);
    sws_freeContext(encode->sws_ctx);

    return 0;
}

static int is_stop;

static void encode_stop(int signo)
{
    printf("\nloops! stop!!!\n");
    is_stop = 1;
}
//
//int main(int argc, char* argv[]) {
//	int got_picture = 0;
//
//	signal(SIGINT, encode_stop);
//	Encode ecd, *encode;
//	encode = &ecd;
//	Camera *camera_s, cam;
//	camera_s = &cam;
//
//	camera_s = calloc_camera();
//	camera_s->width = 640;
//	camera_s->height = 480;
//	camera_s->type = V4L2_PIX_FMT_YUYV;
////	sleep(1);
//	encode->in_w = camera_s->width;
//	encode->in_h = camera_s->height;	//宽高
//	int i;
////	encode->out_file = "src01.h264";					//输出文件路径
//
//	captureInit(camera_s);
//	ffmpeg_init(encode);
//	encode->tmp_file_fd = fopen("tmp", "w");
//
//	for (i = 0; !is_stop; i++) {
//		av_init_packet(&encode->pkt);
//		encode->pkt.data = NULL;
//		encode->pkt.size = 0;
//		//读入YUV
//		usleep(10000);
//		while (read_frame(camera_s, encode->src_data[0]))
//			;
//		sws_scale(encode->sws_ctx, (const uint8_t * const *) (encode->src_data),
//				encode->src_linesize, 0, encode->pCodecCtx->height,
//				encode->picture->data, encode->picture->linesize);
//		//PTS
//		encode->picture->pts = i;
//		//编码
//		int ret = avcodec_encode_video2(encode->pCodecCtx, &encode->pkt,
//				encode->picture, &got_picture);
//		if (ret < 0) {
//			fprintf(stderr, "编码错误！\n");
//			return -1;
//		}
//		if (got_picture == 1) {
//			printf("编码成功1帧！\n");
////			fwrite(encode->pkt.data, 1, encode->pkt.size, encode->tmp_file_fd);
//			uint8_t *poutbuf;
//			int poutbuf_size;
//
//			int a = av_bitstream_filter_filter(encode->bsfc, encode->pCodecCtx,
//			NULL, &poutbuf, &poutbuf_size, encode->pkt.data, encode->pkt.size,
//					0);
//			if (a < 0) {
//				fprintf(stderr, "bitstream_filter failed!\n");
//			} else {
//				fwrite(poutbuf, 1, poutbuf_size, encode->tmp_file_fd);
//			}
//
//			av_free_packet(&encode->pkt);
//		}
//	}
//
//	ffmpeg_encode_end(encode);
//	stop_capturing(camera_s);
//	av_bitstream_filter_close(encode->bsfc);
//	fclose(encode->tmp_file_fd);
//
//	return 0;
//}
