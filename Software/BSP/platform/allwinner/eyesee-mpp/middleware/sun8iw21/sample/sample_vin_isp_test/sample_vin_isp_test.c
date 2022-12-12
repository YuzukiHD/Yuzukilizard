
/*
 ******************************************************************************
 *
 * snapshot.c
 *
 * Hawkview ISP - snapshot.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/17	VIDEO INPUT
 *
 *****************************************************************************
 */

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "isp.h"
#include "video.h"
#include "isp_dev.h"

#include "sunxi_display2.h"

#define MAX_VIDEO_NUM 	4
#define ISP_SERVER_STOP 0
#define MEDIA_DEVICE "/dev/media0"

struct hw_isp_media_dev *media = NULL;
#define ISP_FLASH_TEST 0
#define TEST_ORL 1

char path_name[30];
int save_cnt;
int test_cnt = 10;
int fps = 30;
int wdr_mode = 0;

struct vi_info {
	int Chn;
	int isp_id;
	int s32MilliSec;
	struct video_fmt vfmt;
	struct video_buffer buffer;
};

struct disp_screen {
	int x;
	int y;
	int w;
	int h;
};

int dispfh;
unsigned long arg[4];
disp_layer_config config;

static struct disp_screen get_disp_screen(int w1, int h1, int w2, int h2)
{
	struct disp_screen screen;
	float r1, r2;

	r1 = (float)w1 / (float)w2;
	r2 = (float)h1 / (float)h2;
	if (r1 < r2) {
		screen.w = w2 * r1;
		screen.h = h2 * r1;
	} else {
		screen.w = w2 * r2;
		screen.h = h2 * r2;
	}

	screen.x = (w1 - screen.w) / 2;
	screen.y = (h1 - screen.h) / 2;

	return screen;
}

static int disp_exit(void)
{
	//1.close layer channel 0, prevent back on the first few frames freen
	memset(&config, 0, sizeof(disp_layer_config));
	config.channel = 0;
	config.layer_id = 0;
	config.enable = 0;
	arg[0] = 0;
	arg[1] = (unsigned long)&config;
	arg[2] = 1;
	arg[3] = 0;
	ioctl(dispfh, DISP_LAYER_SET_CONFIG, (void *)arg);

	//1.close layer channel 2
	memset(&config, 0, sizeof(disp_layer_config));
	config.channel = 2;
	config.layer_id = 0;
	config.enable = 0;
	arg[0] = 0;
	arg[1] = (unsigned long)&config;
	arg[2] = 1;
	arg[3] = 0;
	ioctl(dispfh, DISP_LAYER_SET_CONFIG, (void *)arg);

	return 0;
}

static void terminate(int sig_no)
{
	printf("Got signal %d, exiting ...\n", sig_no);
	disp_exit();
	usleep(20*1000);
	exit(1);
}

static void install_sig_handler(void)
{
	signal(SIGBUS, terminate);
	signal(SIGFPE, terminate);
	signal(SIGHUP, terminate);
	signal(SIGILL, terminate);
	signal(SIGKILL, terminate);
	signal(SIGINT, terminate);
	signal(SIGIOT, terminate);
	signal(SIGPIPE, terminate);
	signal(SIGQUIT, terminate);
	signal(SIGSEGV, terminate);
	signal(SIGSYS, terminate);
	signal(SIGTERM, terminate);
	signal(SIGTRAP, terminate);
	signal(SIGUSR1, terminate);
	signal(SIGUSR2, terminate);
}

static int disp_init(int width, int height)
{
	int screen_wdith = 0, screen_height = 0;

	dispfh = open("/dev/disp", O_RDWR);

	disp_exit();

	/* 2. get layer config info */
	memset(&config, 0, sizeof(disp_layer_config));
	config.channel = 0;
	config.layer_id = 0;
	arg[0] = 0;
	arg[1] = (unsigned long)&config;
	arg[2] = (unsigned long)1;
	arg[3] = 0;
	ioctl(dispfh, DISP_LAYER_GET_CONFIG, (void *)arg);
	config.info.fb.format = DISP_FORMAT_YUV420_P;
	config.info.mode	  = LAYER_MODE_BUFFER;
	config.enable = 1;

	screen_wdith = ioctl(dispfh, DISP_GET_SCN_WIDTH, (void *)arg);
	screen_height = ioctl(dispfh, DISP_GET_SCN_HEIGHT, (void *)arg);

	if (screen_wdith < width || screen_height < height) {
		printf("input size is larger than screen, disp_init error!\n");
	}

	config.info.fb.size[0].width    = width;
	config.info.fb.size[0].height	= height;
	config.info.fb.size[1].width    = config.info.fb.size[0].width / 2;
	config.info.fb.size[1].height	= config.info.fb.size[0].height / 2;
	config.info.fb.size[2].width	= config.info.fb.size[0].width / 2;
	config.info.fb.size[2].height	= config.info.fb.size[0].height / 2;

	//crop
	config.info.fb.crop.width  = (unsigned long long)width  << 32;
	config.info.fb.crop.height = (unsigned long long)height << 32;
	//frame
	config.info.alpha_mode	  = 1; //global alpha
	config.info.zorder       = 1;
	config.info.alpha_value       = 0xff;
	if (screen_wdith > width && screen_height > height) {
		config.info.screen_win.x = (screen_wdith - width) / 2;
		config.info.screen_win.y = (screen_height - height) / 2;
		config.info.screen_win.width  = width;
		config.info.screen_win.height = height;
	 } else {
		get_disp_screen(screen_wdith, screen_height, width, height);
		config.info.screen_win.x = 0; /* screen.x; */
		config.info.screen_win.y = 0; /* screen.y; */
		config.info.screen_win.width = screen_wdith;
		config.info.screen_win.height = screen_height;
	 }

	return 0;
}

static int disp_set_addr(u32 width, u32 height, u32 addr0, u32 addr1, u32 addr2)
{
	config.info.fb.size[0].width = width;
	config.info.fb.size[0].height = height;
	config.info.fb.size[1].width = width / 2;
	config.info.fb.size[1].height = height / 2;
	config.info.fb.size[2].width = width / 2;
	config.info.fb.size[2].height = height / 2;
	config.info.fb.crop.height = (unsigned long long)height << 32;
	config.info.fb.crop.height = (unsigned long long)height << 32;

	config.info.fb.addr[0] = addr0;
	config.info.fb.addr[1] = addr1;
	config.info.fb.addr[2] = addr2;

	arg[0] = 0;
	arg[1] = (unsigned long)&config;
	arg[2] = 1;
	arg[3] = 0;
	return ioctl(dispfh, DISP_LAYER_SET_CONFIG, (void *)arg);
}

int osd_set_fmt(int ViCh)
{
	struct isp_video_device *video = NULL;
	struct osd_fmt ofmt;
	void *bitmap = NULL;
	int i, j, bitmap_size = 0, *databuf = NULL;

	if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == media->video_dev[ViCh]) {
		ISP_ERR("vi channel number %d is invalid!\n", ViCh);
		return -1;
	} else {
		video = media->video_dev[ViCh];
	}
#if 0
	ofmt.clipcount = 8;
	ofmt.chromakey = V4L2_PIX_FMT_RGB32;
	ofmt.global_alpha = 255;

	//overlay
	for (i = 0; i < ofmt.clipcount; i++) {
		ofmt.region[i].height = 16;
		ofmt.region[i].width = 16;
		ofmt.region[i].left = 16*i;
		ofmt.region[i].top = 16*i;

		bitmap_size = ofmt.region[i].width * ofmt.region[i].height;

		ofmt.bitmap[i] = calloc(bitmap_size, 4);
		if (ofmt.bitmap[i] == NULL) {
			printf("calloc of bitmap buf failed\n");
			return -ENOMEM;
		}
		databuf = ofmt.bitmap[i];

		for (j = 0; j < bitmap_size; j++) {
			databuf[j] = 0xff000000 | (0xff << ((i % 3) * 8));
		}

		ofmt.reverse_close[i] = 0;
		ofmt.glb_alpha[i] = 16;

		ofmt.inv_th = 80;
		ofmt.inv_w_rgn[i] = 0;
		ofmt.inv_h_rgn[i] = 0;
	}

	overlay_set_fmt(video, &ofmt);

	for (i = 0; i < ofmt.clipcount; i++)
		free(ofmt.bitmap[i]);

	//cover
	for (i = 0; i < ofmt.clipcount; i++) {
		ofmt.region[i].height = 16;
		ofmt.region[i].width = 16;
		ofmt.region[i].left = 32 * (i + 1);
		ofmt.region[i].top = 0;

		ofmt.bitmap[i] = NULL;

		ofmt.rgb_cover[i] = 0x808080;
	}
	overlay_set_fmt(video, &ofmt);
#endif
	//orl
	ofmt.clipcount = 16;
	for (i = 0; i < ofmt.clipcount; i++) {
		ofmt.region[i].height = 16;
		ofmt.region[i].width = 16;
		ofmt.region[i].left = 16 * i;
		ofmt.region[i].top = 16 *i;

		ofmt.bitmap[i] = NULL;

		if(i < 8){
			ofmt.rgb_cover[i] = 0xff000000 | (0xff << ((i % 3) * 8));
		}
	}
	ofmt.width = 2;
	overlay_set_fmt(video, &ofmt);
	return 0;
}

int osd_update(int ViCh, int on_off)
{
	struct isp_video_device *video = NULL;
	if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == media->video_dev[ViCh]) {
		ISP_ERR("vi channel number %d is invalid!\n", ViCh);
		return -1;
	} else {
		video = media->video_dev[ViCh];
	}

	return overlay_update(video, on_off);
}

static void isp_server_start(int isp_id)
{
	/*
	 * media_open must after AW_MPI_VI_InitCh, otherwise, media_pipeline_get_head
	 * will not get sensor entity, and then lead to failure of calling isp_sensor_get_configs,
	 * because of sensor link will not enable until s_input is called.
	 */

	isp_init(isp_id);
	isp_run(isp_id);
}

static void isp_server_stop(int isp_id)
{
	isp_stop(isp_id);
	isp_pthread_join(isp_id);
	isp_exit(isp_id);
}

static void isp_server_wait_to_exit(int isp_id)
{
	isp_pthread_join(isp_id);
	isp_exit(isp_id);
}

static void *loop_cap(void *pArg)
{
	struct vi_info *privCap = (struct vi_info *)pArg;
	struct isp_video_device *video = NULL;
	struct buffers_pool *pool = NULL;
	isp_image_params_t isp_image_params;
	int i = 0;

	printf("current vi channel is = %d\n", privCap->Chn);

#if TEST_ORL
	osd_set_fmt(privCap->Chn);
	osd_update(privCap->Chn, 1);
#endif

	if (privCap->Chn >= HW_VIDEO_DEVICE_NUM || NULL == media->video_dev[privCap->Chn]) {
		ISP_ERR("vi channel number %d is invalid!\n", privCap->Chn);
		return NULL;
	} else {
		video = media->video_dev[privCap->Chn];
	}

	pool = buffers_pool_new(video);
	if (NULL == pool)
		return NULL;

	if (video_req_buffers(video, pool) < 0)
		return NULL;
	video_get_fmt(video, &privCap[privCap->Chn].vfmt);
	for (i = 0; i < privCap[privCap->Chn].vfmt.nbufs; i++)
		video_queue_buffer(video, i);

	//video_event_subscribe(video, V4L2_EVENT_VSYNC);

	if (video_stream_on(video) < 0)
		return NULL;

	i = 0;
	memset(&isp_image_params, 0, sizeof isp_image_params);
	while (i < save_cnt) {
		if (video_wait_buffer(video, privCap->s32MilliSec) < 0)
			goto disablech;

		if (video_dequeue_buffer(video, &privCap->buffer) < 0)
			goto disablech;

		i++;
		printf("process channel(%d) frame %d\r\n", privCap->Chn, i);
		if (save_cnt < 10000) {
			if (test_cnt < 10) {
				if (video_save_frames(video, privCap->buffer.index, path_name) < 0)
					goto disablech;
			}
		} else {
			disp_set_addr(privCap->vfmt.format.width, privCap->vfmt.format.height,
				privCap->buffer.planes[0].mem_phy, privCap->buffer.planes[1].mem_phy, privCap->buffer.planes[2].mem_phy);
			i = i % save_cnt;
		}

		if (video_queue_buffer(video, privCap->buffer.index) < 0)
			goto disablech;

#if ISP_SERVER_STOP
		if (i == save_cnt/4) {
			printf("isp%d server stop!!!\n", privCap->isp_id);
			isp_server_stop(privCap->isp_id);
		}

		if (i == save_cnt/2) {
			printf("isp%d server restart!!!\n", privCap->isp_id);
			isp_server_start(privCap->isp_id);
		}

		if (i == save_cnt*3/4) {
			printf("isp%d server stop!!!\n", privCap->isp_id);
			isp_server_stop(privCap->isp_id);
		}

		if (i == save_cnt*3/5) {
			printf("isp%d server update!!!\n", privCap->isp_id);
			isp_update(privCap->isp_id);
		}
#endif

#if 0
		if (i == save_cnt/5) {
			video_set_control(video, V4L2_CID_AUTOGAIN, 0);
			video_set_control(video, V4L2_CID_GAIN, 32);
		}

		if (i == save_cnt*2/5) {
			video_set_control(video, V4L2_CID_ISO_SENSITIVITY_AUTO, V4L2_ISO_SENSITIVITY_MANUAL);
			video_set_control(video, V4L2_CID_ISO_SENSITIVITY, 4);
		}

		if (i % 100 < 50)
			video_set_control(video, V4L2_CID_SCENE_MODE, 0);
		else
			video_set_control(video, V4L2_CID_SCENE_MODE, 1);
#endif
		//video_set_control(video, V4L2_CID_CONTRAST, i % 128 - 64);
		//video_set_control(video, V4L2_CID_BRIGHTNESS, (i % 128) - 64);
		//video_set_control(video, V4L2_CID_SHARPNESS, i % 32 - 16);

#if ISP_FLASH_TEST
#if 0
		video_set_control(video, V4L2_CID_FLASH_LED_MODE, FLASH_MODE_TORCH);
#else
		//video_set_control(video, V4L2_CID_FLASH_LED_MODE, FLASH_MODE_AUTO); /*linux-4.9*/
		video_set_control(video, V4L2_CID_FLASH_LED_MODE_V1, V4L2_FLASH_MODE_AUTO);/*linux-5.4*/
#endif
		video_set_control(video, V4L2_CID_TAKE_PICTURE, V4L2_TAKE_PICTURE_FLASH);

		isp_get_imageparams(privCap->isp_id, &isp_image_params);
		if (isp_image_params.isp_image_params.image_para.bits.flash_ok || (i == 30)) {
			printf("isp_get_imageparams: flash_ok:%d, i:%d\n", isp_image_params.isp_image_params.image_para.bits.flash_ok, i);
			video_set_control(video, V4L2_CID_TAKE_PICTURE, V4L2_TAKE_PICTURE_STOP);
			video_set_control(video, V4L2_CID_FLASH_LED_MODE, FLASH_MODE_OFF);
			break;
		}
 #endif

#if 0
		int hl0 = 0, bl0 = 0, hl1 = 0, bl1 = 0;
		//hl0 = (i % 64) - 32;
		//isp_set_attr_cfg(privCap->isp_id, ISP_CTRL_HIGH_LIGHT, &hl0);
		bl0 = (i % 64) - 32;
		isp_set_attr_cfg(privCap->isp_id, ISP_CTRL_BACK_LIGHT, &bl0);
		isp_get_attr_cfg(privCap->isp_id, ISP_CTRL_BACK_LIGHT, &bl1);
		printf("ISP_CTRL_BACK_LIGHT level %d %d!!!!!!!!!!\n", bl0, bl1);
#endif
		//struct video_event vi_event;
		//if (video_wait_event(video) < 0)
		//	return -1;
		//video_dequeue_event(video, &vi_event);
	}
disablech:
#if TEST_ORL
	osd_update(privCap->Chn, 0);
#endif
	//video_event_unsubscribe(video, V4L2_EVENT_VSYNC);
	if (video_stream_off(video) < 0)
		return NULL;
	if (video_free_buffers(video) < 0)
		return NULL;
	buffers_pool_delete(video);
	goto vi_exit;
vi_exit:
	return NULL;
}

int main_test(int ch_num, int width, int height, int out_fmt)
{
	pthread_t thid[MAX_VIDEO_NUM];
	int ret, i, ch = -1;
	struct vi_info privCap[MAX_VIDEO_NUM];

	if (media == NULL) {
		media = isp_md_open(MEDIA_DEVICE);
		if (media == NULL) {
			ISP_PRINT("unable to open media device %s\n", MEDIA_DEVICE);
			return -1;
		}
	} else {
		ISP_PRINT("mpi_vi already init\n");
	}

	media_dev_init();

	if (ch_num > MAX_VIDEO_NUM)
		ch_num = MAX_VIDEO_NUM;

	if (ch_num == 0 || ch_num == 1) {
		ch = ch_num;
		ch_num = 2;
	}

	for(i = 0; i < ch_num; i++) {
		if (i != ch && ch != -1)
			continue;
		memset(&privCap[i], 0, sizeof(struct vi_info));
		/*Set Dev ID and Chn ID*/
		privCap[i].Chn = i;
		privCap[i].s32MilliSec = 2000;

		privCap[i].vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		privCap[i].vfmt.memtype = V4L2_MEMORY_MMAP;
		switch (out_fmt) {
		case 0: privCap[i].vfmt.format.pixelformat = V4L2_PIX_FMT_SBGGR8; break;
		case 1: privCap[i].vfmt.format.pixelformat = V4L2_PIX_FMT_YUV420M; break;
		case 2: privCap[i].vfmt.format.pixelformat = V4L2_PIX_FMT_YUV420; break;
		case 3: privCap[i].vfmt.format.pixelformat = V4L2_PIX_FMT_NV12M; break;
		default: privCap[i].vfmt.format.pixelformat = V4L2_PIX_FMT_YUV420M; break;
		}
		privCap[i].vfmt.format.field = V4L2_FIELD_NONE;
		privCap[i].vfmt.format.width = width;
		privCap[i].vfmt.format.height = height;
		privCap[i].vfmt.nbufs = 8;
		privCap[i].vfmt.nplanes = 3;
		privCap[i].vfmt.fps = fps;
		privCap[i].vfmt.capturemode = V4L2_MODE_VIDEO;
		privCap[i].vfmt.use_current_win = 0;
		privCap[i].vfmt.wdr_mode = wdr_mode;

		if (isp_video_open(media, i) < 0) {
			printf("isp_video_open vi%d failed\n", i);
			return -1;
		}

		if (video_set_fmt(media->video_dev[i], &privCap[i].vfmt) < 0) {
			printf("video_set_fmt failed\n");
			return -1;
		}

		video_get_fmt(media->video_dev[i], &privCap[i].vfmt);

		privCap[i].isp_id = video_to_isp_id(media->video_dev[i]);
		if (privCap[i].isp_id == -1)
			continue;

		/*Call isp server*/
		printf("isp%d server start!!!\n", privCap[i].isp_id);
		isp_server_start(privCap[i].isp_id);

		/*Call Video Thread*/
		ret = pthread_create(&thid[i], NULL, loop_cap, (void *)&privCap[i]);
		if (ret < 0) {
			printf("pthread_create loop_cap Chn[%d] failed.\n", i);
			continue;
		}
	}

#if !ISP_SERVER_STOP
	for(i = 0; i < ch_num; i++) {
		if (i != ch && ch != -1)
			continue;
		printf("isp%d server wait to exit!!!\n", privCap[i].isp_id);
		isp_server_wait_to_exit(privCap[i].isp_id);
	}
#endif
	media_dev_exit();

	for(i = 0; i < ch_num; i++) {
		if (i != ch && ch != -1)
			continue;
		printf("video%d wait to exit!!!\n", i);
		pthread_join(thid[i], NULL);
	}

	for(i = 0; i < ch_num; i++) {
		if (i != ch && ch != -1)
			continue;
		isp_video_close(media, i);
	}

	if (media)
		isp_md_close(media);
	media = NULL;

	return 0;
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
	int ret, n = 0;
	int ch_num = 0;
	int width = 640;
	int height = 480;
	int out_fmt = 1;

	//install_sig_handler();

	memset(path_name, 0, sizeof (path_name));

	if (argc == 1) {
		sprintf(path_name, "/mnt/sdcard");
	} else if (argc == 2) {
		sprintf(path_name, "/mnt/sdcard");
		ch_num = atoi(argv[1]);
	} else if (argc == 4) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "/mnt/sdcard");
	} else if (argc == 5) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "%s", argv[4]);
	} else if (argc == 6) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "%s", argv[4]);
		out_fmt = atoi(argv[5]);
	} else if (argc == 7) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "%s", argv[4]);
		out_fmt = atoi(argv[5]);
		save_cnt = atoi(argv[6]);
	} else if (argc == 8) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "%s", argv[4]);
		out_fmt = atoi(argv[5]);
		save_cnt = atoi(argv[6]);
		test_cnt = atoi(argv[7]);
	} else if (argc == 9) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "%s", argv[4]);
		out_fmt = atoi(argv[5]);
		save_cnt = atoi(argv[6]);
		test_cnt = atoi(argv[7]);
		fps = atoi(argv[8]);
	} else if (argc == 10) {
		ch_num = atoi(argv[1]);
		width = atoi(argv[2]);
		height = atoi(argv[3]);
		sprintf(path_name, "%s", argv[4]);
		out_fmt = atoi(argv[5]);
		save_cnt = atoi(argv[6]);
		test_cnt = atoi(argv[7]);
		fps = atoi(argv[8]);
		wdr_mode = atoi(argv[9]);
	} else {
		printf("please select the ch_num: 1~4 ......\n");
		scanf("%d", &ch_num);

		printf("please input the resolution: width height......\n");
		scanf("%d %d", &width, &height);

		printf("please input the frame saving path......\n");
		scanf("%15s", path_name);

		printf("please input the test out_fmt: 0~3......\n");
		scanf("%d", &out_fmt);

		printf("please input the save_cnt: >=1......\n");
		scanf("%d", &save_cnt);
	}

	//disp_init(width, height);

	for (n = 0; n < test_cnt; n++) {
		ret = main_test(ch_num, width, height, out_fmt);
		printf("vin isp test %d, return %d\n", n + 1, ret);
	}

	//disp_exit();

	return 0;
}

