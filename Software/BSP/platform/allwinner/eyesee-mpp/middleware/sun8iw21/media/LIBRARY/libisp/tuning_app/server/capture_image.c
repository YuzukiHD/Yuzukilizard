#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../isp.h"
#include "../../include/device/isp_dev.h"
#include "../../include/device/video.h"
#include "../log_handle.h"
#include "../thread_pool.h"
#include "../socket_protocol.h"
#include "capture_image.h"
#include "server_core.h"
#include "raw_flow_opt.h"
#include "isp_handle.h"



#define CAPTURE_CHANNEL_MAX     HW_VIDEO_DEVICE_NUM
#define CAPTURE_RAW_FLOW_QUEUE_SIZE   201


struct hw_isp_media_dev *g_media_dev = NULL;
int                      g_sensor_set_flag;  // bit0-CAPTURE_CHANNEL_MAX-1 set flag vich 0~CAPTURE_CHANNEL_MAX-1

typedef struct _frame_buffer_info_s
{
	unsigned int	u32PoolId;
	unsigned int	u32Width;
	unsigned int	u32Height;
	unsigned int	u32PixelFormat;
	unsigned int	u32field;

	unsigned int	u32PhyAddr[3];
	void			*pVirAddr[3];
	unsigned int	u32Stride[3];

	struct timeval stTimeStamp;
} frame_buf_info;

typedef struct _vi_priv_attr_s
{
	int vich;
	int timeout;
	struct video_fmt vi_fmt;
	frame_buf_info vi_frame_info;
} vi_priv_attr;

typedef enum _cap_flag_e {
	CAP_STATUS_ON            = 0x0001,  // 0:cap video off, 1:cap video on
	CAP_THREAD_RUNNING       = 0x0002,  // 0:thread stopped, 1:thread running
	CAP_THREAD_STOP          = 0x0004,  // 0: not stop thread, 1:to stop thread
	CAP_RAW_FLOW_RUNNING     = 0x0008,  // 0: raw flow stopped, 1:raw flow running
	CAP_RAW_FLOW_START       = 0x0010,  // 0: stop raw flow, 1:to start raw flow
} cap_flag;

typedef struct _capture_params_s {
	vi_priv_attr             priv_cap;
	unsigned int             status;
	int                      frame_count;
	pthread_mutex_t          locker;
} capture_params;

typedef enum _cap_init_status_e {
	CAPTURE_INIT_NOT         = 0,
	CAPTURE_INIT_YES         = 1,
} cap_init_status;

cap_init_status              g_cap_init_status = CAPTURE_INIT_NOT;
capture_params               g_cap_handle[CAPTURE_CHANNEL_MAX];
pthread_mutex_t              g_cap_locker;

static void reset_cap_params(capture_params *cap_pa)
{
	if (cap_pa) {
		memset(&cap_pa->priv_cap, 0, sizeof(vi_priv_attr));
		cap_pa->status = 0;
		cap_pa->frame_count = 0;
	}
}

#define VALID_VIDEO_SEL(id)    \
	((id) >= 0 && (id) < CAPTURE_CHANNEL_MAX)

static int init_video(struct hw_isp_media_dev *media_dev, int vich)
{
	return isp_video_open(media_dev, vich);
}

static int exit_video(struct hw_isp_media_dev *media_dev, int vich)
{
	isp_video_close(media_dev, vich);
	return 0;
}

static int set_video_fmt(struct hw_isp_media_dev *media_dev, int vich, struct video_fmt *vi_fmt)
{
	struct isp_video_device *video = NULL;

	if (!VALID_VIDEO_SEL(vich) || NULL == media_dev->video_dev[vich]) {
		LOG("%s: invalid vin ch(%d)\n", __FUNCTION__, vich);
		return -1;
	} else {
		video = media_dev->video_dev[vich];
	}

	return video_set_fmt(video, vi_fmt);
}

static int get_video_fmt(struct hw_isp_media_dev *media_dev, int vich, struct video_fmt *vi_fmt)
{
	struct isp_video_device *video = NULL;

	if (!VALID_VIDEO_SEL(vich) || NULL == media_dev->video_dev[vich]) {
		LOG("%s: invalid vin ch(%d)\n", __FUNCTION__, vich);
		return -1;
	} else {
		video = media_dev->video_dev[vich];
	}

	memset(vi_fmt, 0, sizeof(struct video_fmt));
	video_get_fmt(video, vi_fmt);

	return 0;
}

static int enable_video(struct hw_isp_media_dev *media_dev, int vich)
{
	struct isp_video_device *video = NULL;
	struct buffers_pool *pool = NULL;
	struct video_fmt vfmt;
	int i;

	if (!VALID_VIDEO_SEL(vich) || NULL == media_dev->video_dev[vich]) {
		LOG("%s: invalid vin ch(%d)\n", __FUNCTION__, vich);
		return -1;
	} else {
		video = media_dev->video_dev[vich];
	}

	pool = buffers_pool_new(video);
	if (!pool) {
		return -1;
	}

	if (video_req_buffers(video, pool) < 0) {
		return -1;
	}

	memset(&vfmt, 0, sizeof(vfmt));
	video_get_fmt(video, &vfmt);
	for (i = 0; i < vfmt.nbufs; i++) {
		video_queue_buffer(video, i);
	}

	return video_stream_on(video);
}

static int disable_video(struct hw_isp_media_dev *media_dev, int vich)
{
	struct isp_video_device *video = NULL;

	if (!VALID_VIDEO_SEL(vich) || NULL == media_dev->video_dev[vich]) {
		LOG("%s: invalid vin ch(%d)\n", __FUNCTION__, vich);
		return -1;
	} else {
		video = media_dev->video_dev[vich];
	}

	if (video_stream_off(video) < 0) {
		return -1;
	}
	if (video_free_buffers(video) < 0) {
		return -1;
	}
	buffers_pool_delete(video);

	return 0;
}

static int get_video_frame(struct hw_isp_media_dev *media_dev, int vich, frame_buf_info *frame_info, int timeout)
{
	struct isp_video_device *video = NULL;
	struct video_buffer buffer;
	struct video_fmt vfmt;
	int i;

	if (!VALID_VIDEO_SEL(vich) || NULL == media_dev->video_dev[vich]) {
		LOG("%s: invalid vin ch(%d)\n", __FUNCTION__, vich);
		return -1;
	} else {
		video = media_dev->video_dev[vich];
	}

	if (video_wait_buffer(video, timeout) < 0) {
		return -1;
	}

	if (video_dequeue_buffer(video, &buffer) < 0) {
		return -1;
	}

	memset(&vfmt, 0, sizeof(vfmt));
	video_get_fmt(video, &vfmt);
	for (i = 0; i < vfmt.nplanes; i++) {
		frame_info->pVirAddr[i] = buffer.planes[i].mem;
		frame_info->u32Stride[i] = buffer.planes[i].size;
		frame_info->u32PhyAddr[i] = buffer.planes[i].mem_phy;
	}
	frame_info->u32Width = vfmt.format.width;
	frame_info->u32Height = vfmt.format.height;
	frame_info->u32field = vfmt.format.field;
	frame_info->u32PixelFormat = vfmt.format.pixelformat;
	frame_info->stTimeStamp = buffer.timestamp;
	frame_info->u32PoolId = buffer.index;
//	if (ldci_video_sel == TUNINGAPP_VIDEO_IN) {
//		ldci_frame_buf_addr = frame_info->pVirAddr[0];
//	}

	return 0;
}

static int release_video_frame(struct hw_isp_media_dev *media_dev, int vich, frame_buf_info *frame_info)
{
	struct isp_video_device *video = NULL;

	if (!VALID_VIDEO_SEL(vich) || NULL == media_dev->video_dev[vich]) {
		LOG("%s: invalid vin ch(%d)\n", __FUNCTION__, vich);
		return -1;
	} else {
		video = media_dev->video_dev[vich];
	}

	if (video_queue_buffer(video, frame_info->u32PoolId) < 0) {
		return -1;
	}

	return 0;
}

/*
 * frame thread
 */
static void *frame_loop_thread(void *params)
{
	int ret = -1, failed_times = 0, thread_status;
	capture_params *cap_pa = (capture_params *)params;
	//capture_format cap_fmt;
	//cap_fmt.buffer = (unsigned char *)malloc(1 << 24); // 16M
	//unsigned char *buffer = NULL;

	if (cap_pa && g_media_dev) {
		LOG("%s: channel %d starts\n", __FUNCTION__, cap_pa->priv_cap.vich);
		pthread_mutex_lock(&cap_pa->locker);
		cap_pa->status |= CAP_THREAD_RUNNING;
		pthread_mutex_unlock(&cap_pa->locker);
		while (1) {
			msleep(1);
			thread_status = CheckThreadsStatus();
			if (thread_status & TH_STATUS_PREVIEW_VENCODE) {
				msleep(100);
				continue;
			}
			ret = pthread_mutex_trylock(&cap_pa->locker);
			if (0 == ret) { // lock ok
				if (!(CAP_STATUS_ON & cap_pa->status)) {
					cap_pa->status &= ~CAP_THREAD_RUNNING;
					pthread_mutex_unlock(&cap_pa->locker);
					LOG("%s: channel %d is off\n", __FUNCTION__, cap_pa->priv_cap.vich);
					break;
				}
				if (CAP_THREAD_STOP & cap_pa->status) {
					disable_video(g_media_dev, cap_pa->priv_cap.vich);
					exit_video(g_media_dev, cap_pa->priv_cap.vich);
					cap_pa->status &= ~CAP_STATUS_ON;
					cap_pa->status &= ~CAP_THREAD_RUNNING;
					pthread_mutex_unlock(&cap_pa->locker);
					LOG("%s: recv stop flag(channel %d)\n", __FUNCTION__, cap_pa->priv_cap.vich);
					break;
				}
				ret = get_video_frame(g_media_dev, cap_pa->priv_cap.vich, &cap_pa->priv_cap.vi_frame_info, cap_pa->priv_cap.timeout);
				if (ret < 0) {
					LOG("%s: failed to get frame(channel %d, %d)\n", __FUNCTION__, cap_pa->priv_cap.vich, failed_times);
					failed_times++;
					if (failed_times >= 10) {
						disable_video(g_media_dev, cap_pa->priv_cap.vich);
						exit_video(g_media_dev, cap_pa->priv_cap.vich);
						cap_pa->status &= ~CAP_STATUS_ON;
						cap_pa->status &= ~CAP_THREAD_RUNNING;
						pthread_mutex_unlock(&cap_pa->locker);
						LOG("%s: failed too many times(channel %d)\n", __FUNCTION__, cap_pa->priv_cap.vich);
						break;
					}
				} else {
					failed_times = 0;
					#if 0
					if (CAP_RAW_FLOW_START & cap_pa->status) {
						cap_fmt.width = cap_pa->priv_cap.vi_fmt.format.width;
						cap_fmt.height = cap_pa->priv_cap.vi_fmt.format.height;
						cap_fmt.format = cap_pa->priv_cap.vi_fmt.format.pixelformat;
						cap_fmt.planes_count = cap_pa->priv_cap.vi_fmt.nplanes;
						cap_fmt.width_stride[0] = cap_pa->priv_cap.vi_fmt.format.plane_fmt[0].bytesperline;
						cap_fmt.width_stride[1] = cap_pa->priv_cap.vi_fmt.format.plane_fmt[1].bytesperline;
						cap_fmt.width_stride[2] = cap_pa->priv_cap.vi_fmt.format.plane_fmt[2].bytesperline;
						buffer = cap_fmt.buffer;
						cap_fmt.length = 0;
						for (ret = 0; ret < cap_pa->priv_cap.vi_fmt.nplanes; ret++) {
							memcpy(buffer, cap_pa->priv_cap.vi_frame_info.pVirAddr[ret],
								cap_pa->priv_cap.vi_frame_info.u32Stride[ret]);
							buffer += cap_pa->priv_cap.vi_frame_info.u32Stride[ret];
							cap_fmt.length += cap_pa->priv_cap.vi_frame_info.u32Stride[ret];
						}
						if (cap_fmt.length < cap_fmt.width * cap_fmt.height) {
							LOG("%s: raw flow - %d < %dx%d, not matched\n", __FUNCTION__, cap_fmt.length, cap_fmt.width, cap_fmt.height);
						} else {
							queue_raw_flow(&cap_fmt);
							//msleep(8);
						}
						cap_pa->status |= CAP_RAW_FLOW_RUNNING;
					} else {
						cap_pa->status &= ~CAP_RAW_FLOW_RUNNING;
					}
					#endif
					cap_pa->status &= ~CAP_RAW_FLOW_RUNNING;
					release_video_frame(g_media_dev, cap_pa->priv_cap.vich, &cap_pa->priv_cap.vi_frame_info);
				}
				pthread_mutex_unlock(&cap_pa->locker);
			}
		}

		LOG("%s: channel %d quits\n", __FUNCTION__, cap_pa->priv_cap.vich);
	}
	//free(cap_fmt.buffer);
	//cap_fmt.buffer = NULL;

	return 0;
}

/*
 * start video node
 * returns CAP_ERR_NONE if OK, others if something went wrong
 */
int start_video(capture_params *cap_pa, capture_format *cap_fmt)
{
	int ret = CAP_ERR_NONE;
	int fmt_changed = 0;

	if (!cap_pa || !cap_fmt || !VALID_VIDEO_SEL(cap_fmt->channel)) {
		LOG("%s: invalid params\n", __FUNCTION__);
		return CAP_ERR_INVALID_PARAMS;
	}

	// check whether set sensor or not
 	if (!g_sensor_set_flag) {
		LOG("%s: not set sensor input yet(channel %d)\n", __FUNCTION__, cap_fmt->channel);
		return CAP_ERR_NOT_SET_INPUT;
	}

	pthread_mutex_lock(&cap_pa->locker);
	if (CAP_STATUS_ON & cap_pa->status) {
		if (cap_pa->priv_cap.vi_fmt.format.pixelformat != cap_fmt->format ||
			cap_pa->priv_cap.vi_fmt.format.width != cap_fmt->width ||
			cap_pa->priv_cap.vi_fmt.format.height != cap_fmt->height) {
			LOG("%s: vich%d format changes: fmt-%d, %dx%d -> fmt-%d, %dx%d\n", __FUNCTION__,
				cap_fmt->channel,
				cap_pa->priv_cap.vi_fmt.format.pixelformat,
				cap_pa->priv_cap.vi_fmt.format.width,
				cap_pa->priv_cap.vi_fmt.format.height,
				cap_fmt->format, cap_fmt->width, cap_fmt->height);

			disable_video(g_media_dev, cap_fmt->channel);
			exit_video(g_media_dev, cap_fmt->channel);
		} else {
			fmt_changed = 0;
			goto start_video_get_fmt;
		}
	}

	fmt_changed = 1;
	cap_pa->status &= ~CAP_STATUS_ON;

	ret = init_video(g_media_dev, cap_fmt->channel);
	if (ret) {
		ret = CAP_ERR_CH_INIT;
		LOG("%s: failed to init channel %d\n", __FUNCTION__, cap_fmt->channel);
		goto start_video_end;
	}

	cap_pa->priv_cap.vich = cap_fmt->channel;
	cap_pa->priv_cap.timeout = 2000;  // ms
	cap_pa->priv_cap.vi_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap_pa->priv_cap.vi_fmt.memtype = V4L2_MEMORY_MMAP;
	cap_pa->priv_cap.vi_fmt.format.pixelformat = cap_fmt->format;
	cap_pa->priv_cap.vi_fmt.format.field = V4L2_FIELD_NONE;
	cap_pa->priv_cap.vi_fmt.format.width = cap_fmt->width;
	cap_pa->priv_cap.vi_fmt.format.height = cap_fmt->height;
	cap_pa->priv_cap.vi_fmt.nbufs = 3;
	cap_pa->priv_cap.vi_fmt.nplanes = cap_fmt->planes_count;
	cap_pa->priv_cap.vi_fmt.capturemode = V4L2_MODE_VIDEO;
	cap_pa->priv_cap.vi_fmt.fps = cap_fmt->fps;
	cap_pa->priv_cap.vi_fmt.wdr_mode = cap_fmt->wdr;
	cap_pa->priv_cap.vi_fmt.use_current_win = 1;  //!!! not to change sensor input format
	cap_pa->priv_cap.vi_fmt.index = cap_fmt->index;
	cap_pa->priv_cap.vi_fmt.pixel_num = MIPI_ONE_PIXEL;
	cap_pa->priv_cap.vi_fmt.tdm_speed_down_en = 0;

	ret = set_video_fmt(g_media_dev, cap_fmt->channel, &cap_pa->priv_cap.vi_fmt);
	if (ret) {
		exit_video(g_media_dev, cap_fmt->channel);
		ret = CAP_ERR_CH_SET_FMT;
		LOG("%s: failed to set format(channel %d)\n", __FUNCTION__, cap_fmt->channel);
		goto start_video_end;
	}

	ret = enable_video(g_media_dev, cap_fmt->channel);
	if (ret) {
		exit_video(g_media_dev, cap_fmt->channel);
		ret = CAP_ERR_CH_ENABLE;
		LOG("%s: failed to enble channel %d\n", __FUNCTION__, cap_fmt->channel);
		goto start_video_end;
	}

start_video_get_fmt:
	ret = get_video_fmt(g_media_dev, cap_fmt->channel, &cap_pa->priv_cap.vi_fmt);
	if (ret) {
		disable_video(g_media_dev, cap_fmt->channel);
		exit_video(g_media_dev, cap_fmt->channel);
		ret = CAP_ERR_CH_GET_FMT;
		LOG("%s: failed to get format(channel %d)\n", __FUNCTION__, cap_fmt->channel);
		goto start_video_end;
	}

	if (fmt_changed) {
		LOG("%s: vich%d format - fmt-%d, %dx%d@%d, wdr-%d, planes-%d[%d, %d, %d]\n", __FUNCTION__,
			cap_fmt->channel,
			cap_pa->priv_cap.vi_fmt.format.pixelformat,
			cap_pa->priv_cap.vi_fmt.format.width,
			cap_pa->priv_cap.vi_fmt.format.height,
			cap_pa->priv_cap.vi_fmt.fps,
			cap_pa->priv_cap.vi_fmt.wdr_mode,
			cap_pa->priv_cap.vi_fmt.nplanes,
			cap_pa->priv_cap.vi_fmt.format.plane_fmt[0].bytesperline,
			cap_pa->priv_cap.vi_fmt.format.plane_fmt[1].bytesperline,
			cap_pa->priv_cap.vi_fmt.format.plane_fmt[2].bytesperline);
	}

	cap_fmt->width = cap_pa->priv_cap.vi_fmt.format.width;
	cap_fmt->height = cap_pa->priv_cap.vi_fmt.format.height;
	cap_fmt->format = cap_pa->priv_cap.vi_fmt.format.pixelformat;
	cap_fmt->planes_count = cap_pa->priv_cap.vi_fmt.nplanes;
	cap_fmt->width_stride[0] = cap_pa->priv_cap.vi_fmt.format.plane_fmt[0].bytesperline;
	cap_fmt->width_stride[1] = cap_pa->priv_cap.vi_fmt.format.plane_fmt[1].bytesperline;
	cap_fmt->width_stride[2] = cap_pa->priv_cap.vi_fmt.format.plane_fmt[2].bytesperline;

	cap_pa->status |= CAP_STATUS_ON;
	if (!(CAP_THREAD_RUNNING & cap_pa->status)) {
		add_work(&frame_loop_thread, cap_pa);
	}
	ret = CAP_ERR_NONE;

	if (fmt_changed) {
		msleep(1000);
		int isp_id = 0;
		ret = select_isp(isp_id, 1);//PC Tools input isp_id
		if (ret) {
			LOG("%s: failed to select isp %d\n", __FUNCTION__, cap_fmt->channel);
		}
	}

start_video_end:
	pthread_mutex_unlock(&cap_pa->locker);
	if (CAP_ERR_NONE == ret) {
		do {
			msleep(1);
			pthread_mutex_lock(&cap_pa->locker);
			if (CAP_THREAD_RUNNING & cap_pa->status) {
				pthread_mutex_unlock(&cap_pa->locker);
				break;
			}
			pthread_mutex_unlock(&cap_pa->locker);
		} while (1);
	}
	return ret;
}

int init_capture_module()
{
	int i = 0;
	capture_params *cap_handle = NULL;
	struct ToolsIniTuning_cfg ini_cfg = GetIniTuningEn();

	if (CAPTURE_INIT_YES == g_cap_init_status) {
		exit_capture_module();
	}

	if (!ini_cfg.enable) {
		g_media_dev = isp_md_open("/dev/media0");
		if (!g_media_dev) {
			LOG("%s: failed to init media\n", __FUNCTION__);
			return CAP_ERR_MPI_INIT;
		}
	}

	for (i = 0, cap_handle = g_cap_handle; i < CAPTURE_CHANNEL_MAX; i++, cap_handle++) {
		pthread_mutex_init(&cap_handle->locker, NULL);
		reset_cap_params(cap_handle);
	}

	pthread_mutex_init(&g_cap_locker, NULL);
	g_cap_init_status = CAPTURE_INIT_YES;

	LOG("%s: init done\n", __FUNCTION__);
	return CAP_ERR_NONE;
}

int exit_capture_module()
{
	int i = 0;
	capture_params *cap_handle = NULL;
	struct ToolsIniTuning_cfg ini_cfg = GetIniTuningEn();

	LOG("%s: ready to exit\n", __FUNCTION__);

	if (CAPTURE_INIT_YES == g_cap_init_status) {
		pthread_mutex_lock(&g_cap_locker);
		for (i = 0, cap_handle = g_cap_handle; i < CAPTURE_CHANNEL_MAX; i++, cap_handle++) {
			pthread_mutex_lock(&cap_handle->locker);
			cap_handle->status |= CAP_THREAD_STOP;  // set stop
			pthread_mutex_unlock(&cap_handle->locker);
		}
		msleep(32);

		for (i = 0, cap_handle = g_cap_handle; i < CAPTURE_CHANNEL_MAX; i++, cap_handle++) {
			do {
				msleep(32);
				pthread_mutex_lock(&cap_handle->locker);
				if (!(CAP_THREAD_RUNNING & cap_handle->status)) {
					pthread_mutex_unlock(&cap_handle->locker);
					break;
				}
				pthread_mutex_unlock(&cap_handle->locker);
			} while (1);

			pthread_mutex_lock(&cap_handle->locker);
			if (CAP_STATUS_ON & cap_handle->status) {
				if (!ini_cfg.enable) {
					disable_video(g_media_dev, cap_handle->priv_cap.vich);
					exit_video(g_media_dev, cap_handle->priv_cap.vich);
				}
				cap_handle->status &= ~CAP_STATUS_ON;
			}
			pthread_mutex_unlock(&cap_handle->locker);
			pthread_mutex_destroy(&cap_handle->locker);
		}

		if (!ini_cfg.enable) {
			if (g_media_dev) {
				isp_md_close(g_media_dev);
				g_media_dev = NULL;
			}
		}

		pthread_mutex_unlock(&g_cap_locker);
		pthread_mutex_destroy(&g_cap_locker);
		g_cap_init_status = CAPTURE_INIT_NOT;
	}

	LOG("%s: exits\n", __FUNCTION__);
	return CAP_ERR_NONE;
}

int get_vich_status()
{
	capture_params *cap_handle = NULL;
	int i = 0, ret = 0;
	for (i = 0, cap_handle = g_cap_handle; i < CAPTURE_CHANNEL_MAX; i++, cap_handle++) {
		if (CAP_STATUS_ON & cap_handle->status) {
			ret |= (1 << i);
		}
	}
	return ret;
}

int set_sensor_input(const sensor_input *sensor_in)
{
	capture_params *cap_handle = NULL;
	int ret = CAP_ERR_NONE;

	if (sensor_in && VALID_VIDEO_SEL(sensor_in->channel)) {
		// check whether same format or not
		//sensor_set_flag = g_sensor_set_flag & (1<<sensor_in->channel);
		//LOG("%s: channel %d, set flag %d\n", __FUNCTION__, sensor_in->channel, sensor_set_flag);
		//if (sensor_set_flag) {
		//	return CAP_ERR_NONE;
		//}

		pthread_mutex_lock(&g_cap_locker);
		cap_handle = g_cap_handle + sensor_in->channel;

		pthread_mutex_lock(&cap_handle->locker);
		if (CAP_STATUS_ON & cap_handle->status) {
			disable_video(g_media_dev, sensor_in->channel);
			exit_video(g_media_dev, sensor_in->channel);
		}
		ret = init_video(g_media_dev, sensor_in->channel);
		if (ret) {
			ret = CAP_ERR_CH_INIT;
			LOG("%s: failed to init channel %d\n", __FUNCTION__, sensor_in->channel);
			goto set_sensor_input_end;
		}
		cap_handle->status &= ~CAP_STATUS_ON;

		cap_handle->priv_cap.vich = sensor_in->channel;
		cap_handle->priv_cap.timeout = 2000;
		cap_handle->priv_cap.vi_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		cap_handle->priv_cap.vi_fmt.memtype = V4L2_MEMORY_MMAP;
		//cap_handle->priv_cap.vi_fmt.format.pixelformat = V4L2_PIX_FMT_NV12M;
		cap_handle->priv_cap.vi_fmt.format.pixelformat = sensor_in->format;
		cap_handle->priv_cap.vi_fmt.format.field = V4L2_FIELD_NONE;
		cap_handle->priv_cap.vi_fmt.format.width = sensor_in->width;
		cap_handle->priv_cap.vi_fmt.format.height = sensor_in->height;
		cap_handle->priv_cap.vi_fmt.nbufs = 3;
		cap_handle->priv_cap.vi_fmt.nplanes = 2;
		cap_handle->priv_cap.vi_fmt.capturemode = V4L2_MODE_VIDEO;
		cap_handle->priv_cap.vi_fmt.fps = sensor_in->fps;
		cap_handle->priv_cap.vi_fmt.wdr_mode = sensor_in->wdr;
		cap_handle->priv_cap.vi_fmt.use_current_win = 0;  //!!! try to change sensor input format
		cap_handle->priv_cap.vi_fmt.index = sensor_in->index;
		cap_handle->priv_cap.vi_fmt.pixel_num = MIPI_ONE_PIXEL;
		cap_handle->priv_cap.vi_fmt.tdm_speed_down_en = 0;

//		if (cap_handle->priv_cap.vich) {
//			ISP_PRINT("ldci video from awtuningapp, please set 160*90\n");
//			ldci_video_sel = TUNINGAPP_VIDEO_IN;
//		}

		ret = set_video_fmt(g_media_dev, sensor_in->channel, &cap_handle->priv_cap.vi_fmt);
		if (ret) {
			exit_video(g_media_dev, sensor_in->channel);
			ret = CAP_ERR_CH_SET_FMT;
			LOG("%s: failed to set format(channel %d)\n", __FUNCTION__, sensor_in->channel);
			goto set_sensor_input_end;
		}
		
		ret = enable_video(g_media_dev, sensor_in->channel);
		if (ret) {
			exit_video(g_media_dev, sensor_in->channel);
			ret = CAP_ERR_CH_ENABLE;
			LOG("%s: failed to enable channel %d\n", __FUNCTION__, sensor_in->channel);
			goto set_sensor_input_end;
		}

		ret = get_video_fmt(g_media_dev, sensor_in->channel, &cap_handle->priv_cap.vi_fmt);
		if (ret) {
			disable_video(g_media_dev, sensor_in->channel);
			exit_video(g_media_dev, sensor_in->channel);
			ret = CAP_ERR_CH_GET_FMT;
			LOG("%s: failed to get format(channel %d)\n", __FUNCTION__, sensor_in->channel);
			goto set_sensor_input_end;
		}

		LOG("%s: vich%d format - fmt-%d, %dx%d@%d, wdr-%d, planes-%d[%d, %d, %d]\n", __FUNCTION__,
			sensor_in->channel,
			cap_handle->priv_cap.vi_fmt.format.pixelformat,
			cap_handle->priv_cap.vi_fmt.format.width,
			cap_handle->priv_cap.vi_fmt.format.height,
			cap_handle->priv_cap.vi_fmt.fps,
			cap_handle->priv_cap.vi_fmt.wdr_mode,
			cap_handle->priv_cap.vi_fmt.nplanes,
			cap_handle->priv_cap.vi_fmt.format.plane_fmt[0].bytesperline,
			cap_handle->priv_cap.vi_fmt.format.plane_fmt[1].bytesperline,
			cap_handle->priv_cap.vi_fmt.format.plane_fmt[2].bytesperline);

		g_sensor_set_flag |= (1<<sensor_in->channel);
		cap_handle->status |= CAP_STATUS_ON;
		if (!(CAP_THREAD_RUNNING & cap_handle->status)) {
			add_work(&frame_loop_thread, cap_handle);
		}
		ret = CAP_ERR_NONE;

set_sensor_input_end:
		pthread_mutex_unlock(&cap_handle->locker);
		if (CAP_ERR_NONE == ret) {
			do {
				msleep(1);
				pthread_mutex_lock(&cap_handle->locker);
				if (CAP_THREAD_RUNNING & cap_handle->status) {
					pthread_mutex_unlock(&cap_handle->locker);
					break;
				}
				pthread_mutex_unlock(&cap_handle->locker);
			} while (1);
		}
		pthread_mutex_unlock(&g_cap_locker);
		return ret;		
	}

	return -1;
}


int get_capture_buffer(capture_format *cap_fmt)
{
	int ret = CAP_ERR_NONE;
	unsigned char *buffer = NULL;
	capture_params *cap_handle = NULL;
	unsigned char *uptr = NULL;
	unsigned int i;

	if (cap_fmt && VALID_VIDEO_SEL(cap_fmt->channel)) {
		pthread_mutex_lock(&g_cap_locker);
		cap_handle = g_cap_handle + cap_fmt->channel;
		//LOG("%s: %d\n", __FUNCTION__, __LINE__);
		ret = start_video(cap_handle, cap_fmt);
		//LOG("%s: %d\n", __FUNCTION__, __LINE__);
		if (CAP_ERR_NONE == ret) {
			// get frame
			//LOG("%s: ready to get frame(channel %d)\n", __FUNCTION__, cap_fmt->channel);
			//LOG("%s: %d\n", __FUNCTION__, __LINE__);
			pthread_mutex_lock(&cap_handle->locker);
			//LOG("%s: %d\n", __FUNCTION__, __LINE__);
			ret = get_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info, cap_handle->priv_cap.timeout);
			//LOG("%s: get frame %d done(channel %d)\n", __FUNCTION__, frames_count, cap_fmt->channel);
			cap_handle->frame_count++;
			if (ret < 0) {
				ret = CAP_ERR_GET_FRAME;
				LOG("%s: failed to get frame %d(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
			} else {
				if (cap_handle->frame_count % 50 == 0) {
					LOG("%s: get frame %d done(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
				}
				buffer = cap_fmt->buffer;
				cap_fmt->length = 0;
				for (ret = 0; ret < cap_fmt->planes_count; ret++) {
					memcpy(buffer, cap_handle->priv_cap.vi_frame_info.pVirAddr[ret],
						cap_handle->priv_cap.vi_frame_info.u32Stride[ret]);
					buffer += cap_handle->priv_cap.vi_frame_info.u32Stride[ret];
					cap_fmt->length += cap_handle->priv_cap.vi_frame_info.u32Stride[ret];
				}
#if (ISP_VERSION >= 521)
				if((cap_fmt->format != V4L2_PIX_FMT_SBGGR8) && (cap_fmt->format != V4L2_PIX_FMT_SGBRG8) &&
					(cap_fmt->format != V4L2_PIX_FMT_SGRBG8) && (cap_fmt->format != V4L2_PIX_FMT_SRGGB8) &&
					(cap_fmt->format != V4L2_PIX_FMT_SBGGR10) && (cap_fmt->format != V4L2_PIX_FMT_SGBRG10) &&
					(cap_fmt->format != V4L2_PIX_FMT_SGRBG10) && (cap_fmt->format != V4L2_PIX_FMT_SRGGB10) &&
					(cap_fmt->format != V4L2_PIX_FMT_SBGGR12) && (cap_fmt->format != V4L2_PIX_FMT_SGBRG12) &&
					(cap_fmt->format != V4L2_PIX_FMT_SGRBG12) && (cap_fmt->format != V4L2_PIX_FMT_SRGGB12) &&
					(cap_fmt->format != V4L2_PIX_FMT_LBC_2_0X) && (cap_fmt->format != V4L2_PIX_FMT_LBC_2_5X) &&
					(cap_fmt->format != V4L2_PIX_FMT_LBC_1_0X) && (cap_fmt->format != V4L2_PIX_FMT_LBC_1_5X)) {
					if(cap_fmt->height % 16) { // height ALIGN
						uptr = cap_fmt->buffer + (cap_fmt->width * cap_fmt->height);
						for(i = 0; i < cap_fmt->width * cap_fmt->height / 2; i++) {
							*(uptr + i) = *(uptr + i + (cap_fmt->width * (16 - cap_fmt->height % 16)));
						}
					}
				}
#endif
				//if (cap_fmt->length < cap_fmt->width * cap_fmt->height) {
				//	LOG("%s: %d < %dx%d, not matched\n", __FUNCTION__, cap_fmt->length, cap_fmt->width, cap_fmt->height);
				//	ret = CAP_ERR_GET_FRAME;
				//} else {
					ret = CAP_ERR_NONE;
				//}
				release_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info);
			}
			pthread_mutex_unlock(&cap_handle->locker);
		}
		pthread_mutex_unlock(&g_cap_locker);
	} else {
		ret = CAP_ERR_INVALID_PARAMS;
	}

	return ret;
}

int get_capture_blockinfo(capture_format *cap_fmt, int GrayBlocksFlag, SRegion *region)
{
	int ret = CAP_ERR_NONE;
	unsigned char *buffer = NULL;
	capture_params *cap_handle = NULL;
	//unsigned char *uptr = NULL;
	unsigned int i, j, k, b_width, b_height;

	if (cap_fmt && VALID_VIDEO_SEL(cap_fmt->channel)) {
		pthread_mutex_lock(&g_cap_locker);
		cap_handle = g_cap_handle + cap_fmt->channel;
		//LOG("%s: %d\n", __FUNCTION__, __LINE__);
		ret = start_video(cap_handle, cap_fmt);
		//LOG("%s: %d\n", __FUNCTION__, __LINE__);
		if (CAP_ERR_NONE == ret) {
			// get frame
			//LOG("%s: ready to get frame(channel %d)\n", __FUNCTION__, cap_fmt->channel);
			//LOG("%s: %d\n", __FUNCTION__, __LINE__);
			pthread_mutex_lock(&cap_handle->locker);
			//LOG("%s: %d\n", __FUNCTION__, __LINE__);
			ret = get_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info, cap_handle->priv_cap.timeout);
			//LOG("%s: get frame %d done(channel %d)\n", __FUNCTION__, frames_count, cap_fmt->channel);
			cap_handle->frame_count++;
			if (ret < 0) {
				ret = CAP_ERR_GET_FRAME;
				LOG("%s: failed to get frame %d(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
			} else {
				if (cap_handle->frame_count % 50 == 0) {
					LOG("%s: get frame %d done(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
				}
				buffer = cap_fmt->buffer;
				cap_fmt->length = 0;

				for (i = 0; i < 20 ; i++) {
					if (GrayBlocksFlag & (1 << i)) {
						b_width = region[i].right - region[i].left + 1;
						b_height = region[i].bottom - region[i].top + 1;
						for (j = 0; j < b_height; j++) {
							for (k = 0; k < b_width; k++) {
								*buffer = *((unsigned char *)cap_handle->priv_cap.vi_frame_info.pVirAddr[0] + (region[i].top + j) * cap_fmt->width + region[i].left + k);
								buffer++;
							}
						}
						cap_fmt->length += b_width * b_height;
					}
				}
				//for (ret = 0; ret < cap_fmt->planes_count; ret++) {
				//	memcpy(buffer, cap_handle->priv_cap.vi_frame_info.pVirAddr[ret],
				//		cap_handle->priv_cap.vi_frame_info.u32Stride[ret]);
				//	buffer += cap_handle->priv_cap.vi_frame_info.u32Stride[ret];
				//	cap_fmt->length += cap_handle->priv_cap.vi_frame_info.u32Stride[ret];
				//}

				//if (cap_fmt->length < cap_fmt->width * cap_fmt->height) {
				//	LOG("%s: %d < %dx%d, not matched\n", __FUNCTION__, cap_fmt->length, cap_fmt->width, cap_fmt->height);
				//	ret = CAP_ERR_GET_FRAME;
				//} else {
				ret = CAP_ERR_NONE;
				//}
				release_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info);
			}
			pthread_mutex_unlock(&cap_handle->locker);
		}
		pthread_mutex_unlock(&g_cap_locker);
	} else {
		ret = CAP_ERR_INVALID_PARAMS;
	}

	return ret;
}


#ifdef ANDROID_VENCODE
int set_vencode_config(capture_format *cap_fmt, encode_param_t *encode_param, int type)
{
	if(cap_fmt->width == 0 || cap_fmt->height == 0) {
		LOG("%s: cap_fmt.width=%d height=%d is invaild, please check PC setting or socket comm\n",
			__FUNCTION__, cap_fmt->width, cap_fmt->height);
		return CAP_ERR_INVALID_PARAMS;
	}

	memset(encode_param, 0, sizeof(encode_param_t));
	encode_param->src_width = cap_fmt->width;
	encode_param->src_height = cap_fmt->height;
#if 0
	encode_param->dst_width = cap_fmt->width;
	encode_param->dst_height = cap_fmt->height;
#else
	if (type == SOCK_CMD_VENCODE_ENCPP_YUV) {
		encode_param->dst_width = cap_fmt->width;
		encode_param->dst_height = cap_fmt->height;
	} else {
		if (vencoder_tuning_param->common_cfg.DstPicWidth < cap_fmt->width ||
			vencoder_tuning_param->common_cfg.DstPicHeight < cap_fmt->height) {
			encode_param->dst_width = cap_fmt->width;
			encode_param->dst_height = cap_fmt->height;
		} else {
		    encode_param->dst_width = vencoder_tuning_param->common_cfg.DstPicWidth;
			encode_param->dst_height = vencoder_tuning_param->common_cfg.DstPicHeight;
		}
	}
#endif
	printf("encode_param width=%d height=%d\n", encode_param->src_width, encode_param->src_height);
	encode_param->frame_rate = cap_fmt->fps;
	//encode_param->maxKeyFrame = 30;
	encode_param->maxKeyFrame = vencoder_tuning_param->common_cfg.MaxKeyInterval;
	encode_param->encode_frame_num = 1;

	//if(encode_param->dst_width >= 3840)
	//	encode_param->bit_rate = 20*1024*1024;
	//else if(encode_param->dst_width >= 1920)
	//	encode_param->bit_rate = 10*1024*1024;
	//else if(encode_param->dst_width >= 1280)
	//	encode_param->bit_rate = 6*1024*1024;
	//else if(encode_param->dst_width >= 640)
	//	encode_param->bit_rate = 2*1024*1024;
	//else
	//	encode_param->bit_rate = 1*1024*1024;
	encode_param->bit_rate = vencoder_tuning_param->common_cfg.mBitRate;

	encode_param->qp_min = vencoder_tuning_param->common_cfg.mMixQp;
	encode_param->qp_max = vencoder_tuning_param->common_cfg.mMaxQp;
	encode_param->mInitQp = vencoder_tuning_param->common_cfg.mInitQp;
	encode_param->bFastEncFlag = vencoder_tuning_param->common_cfg.FastEncFlag;
	encode_param->mbPintraEnable = vencoder_tuning_param->common_cfg.mbPintraEnable;
	encode_param->Rotate = vencoder_tuning_param->common_cfg.Rotate;
      encode_param->n3DNR   = vencoder_tuning_param->common_cfg.Flag_3DNR;
	printf("+++++++++++++++3dnr=%d", vencoder_tuning_param->common_cfg.Flag_3DNR);
      encode_param->IQpOffset = vencoder_tuning_param->common_cfg.IQpOffset;
      encode_param->SbmBufSize = vencoder_tuning_param->common_cfg.SbmBufSize;
      encode_param->MaxReEncodeTimes = vencoder_tuning_param->common_cfg.MaxReEncodeTimes;
	if (type == SOCK_CMD_VENCODE_ENCPP_YUV)
		encode_param->encode_format = VENC_CODEC_H264;
	else
		encode_param->encode_format = vencoder_tuning_param->common_cfg.EncodeFormat;

	if (cap_fmt->format == V4L2_PIX_FMT_NV12M) {
		LOG("set_vencode_config: cap_fmt->format: NV12M\n");
		encode_param->picture_format = VENC_PIXEL_YUV420SP;
	} else if (cap_fmt->format == V4L2_PIX_FMT_NV21M) {
		LOG("set_vencode_config: cap_fmt->format: NV21M\n");
		encode_param->picture_format = VENC_PIXEL_YVU420SP;
	} else if (cap_fmt->format == V4L2_PIX_FMT_LBC_1_0X) {
		LOG("set_vencode_config: cap_fmt->format: V4L2_PIX_FMT_LBC_1_0X\n");
		encode_param->picture_format = VENC_PIXEL_LBC_AW;
	} else if (cap_fmt->format == V4L2_PIX_FMT_LBC_1_5X) {
		LOG("set_vencode_config: cap_fmt->format: V4L2_PIX_FMT_LBC_1_5X\n");
		encode_param->picture_format = VENC_PIXEL_LBC_AW;
		encode_param->bLbcLossyComEnFlag1_5x = 1;
	} else if (cap_fmt->format == V4L2_PIX_FMT_LBC_2_0X) {
		LOG("set_vencode_config: cap_fmt->format: V4L2_PIX_FMT_LBC_2_0X\n");
		encode_param->picture_format = VENC_PIXEL_LBC_AW;
		encode_param->bLbcLossyComEnFlag2x = 1;
	} else if (cap_fmt->format == V4L2_PIX_FMT_LBC_2_5X) {
		LOG("set_vencode_config: cap_fmt->format: V4L2_PIX_FMT_LBC_2_5X\n");
		encode_param->picture_format = VENC_PIXEL_LBC_AW;
		encode_param->bLbcLossyComEnFlag2_5x = 1;
	} else {
		LOG("set_vencode_config: cap_fmt->format: unknow or unknow support\n");
		encode_param->picture_format = VENC_PIXEL_YUV420SP;
	}

	encode_param->fixqp.mIQp = vencoder_tuning_param->fixqp_cfg.mIQp;
	encode_param->fixqp.mPQp = vencoder_tuning_param->fixqp_cfg.mPQp;

#ifdef INPUTSOURCE_FILE
	unsigned int nAlignW, nAlignH;

	encode_param->encode_frame_num = 30;
	memset(&encode_param->bufferParam, 0 ,sizeof(VencAllocateBufferParam));
	//* ve require 16-align
	nAlignW = (encode_param->src_width + 15)& ~15;
	nAlignH = (encode_param->src_height + 15)& ~15;
	encode_param->bufferParam.nSizeY = nAlignW*nAlignH;
	encode_param->bufferParam.nSizeC = nAlignW*nAlignH/2;
	encode_param->bufferParam.nBufferNum = 1;
	/******** end set bufferParam param********/

	encode_param->picture_format = VENC_PIXEL_YUV420P;
	encode_param->src_size = encode_param->src_width * encode_param->src_height;
	encode_param->dts_size = encode_param->src_size;
	strcpy((char*)encode_param->input_path,  "/tmp/1080.yuv");
	strcpy((char*)encode_param->output_path, "/tmp/1080p.h264");
	encode_param->in_file = fopen(encode_param->input_path, "r");
	if(encode_param->in_file == NULL)
	{
		printf("open in_file fail\n");
		return CAP_ERR_INVALID_PARAMS;
	}

	encode_param->out_file = fopen(encode_param->output_path, "wb");
	if(encode_param->out_file == NULL)
	{
		printf("open out_file fail\n");
		fclose(encode_param->in_file);
		return CAP_ERR_INVALID_PARAMS;
	}
#endif

	encode_param->debug_gdc_en = vencoder_tuning_param->savebsfile_cfg.Save_bsfile_flag;
	return CAP_ERR_NONE;
}

int get_capture_vencode_buffer(capture_format *cap_fmt, encode_param_t *encode_param, int type)
{
	int i;
	int ret = CAP_ERR_NONE;
	capture_params *cap_handle = NULL;
	unsigned char *buffer = cap_fmt->buffer;
	VencInputBuffer *inputBuffer = &encode_param->inputBuffer;
	VencOutputBuffer *outputBuffer = &encode_param->outputBuffer;
	unsigned char *uptr = NULL;

	if (cap_fmt && VALID_VIDEO_SEL(cap_fmt->channel)) {
		if (type == SOCK_CMD_VENCODE_PPSSPS) {
			ret = EncoderStart(encode_param, NULL, NULL, VENCODE_CMD_HEAD_PPSSPS);
			if (ret) {
				LOG("%s: detect encode error!!@%d\n", __FUNCTION__, __LINE__);
				ret = CAP_ERR_VENCODE_PPSSPS;
			} else {
				//Encoder data save in sps_pps_data
				cap_fmt->length = encode_param->sps_pps_data.nLength;
				memcpy(buffer, encode_param->sps_pps_data.pBuffer, cap_fmt->length);
				ret = CAP_ERR_NONE;
			}
		} else if (type == SOCK_CMD_VENCODE_STREAM) {
			pthread_mutex_lock(&g_cap_locker);
			cap_handle = g_cap_handle + cap_fmt->channel;
			ret = start_video(cap_handle, cap_fmt);
			if (CAP_ERR_NONE == ret) {
				pthread_mutex_lock(&cap_handle->locker);
				ret = get_video_frame(g_media_dev, cap_handle->priv_cap.vich,
									  &cap_handle->priv_cap.vi_frame_info,
									  cap_handle->priv_cap.timeout);
				cap_handle->frame_count++;
				if (ret < 0) {
					ret = CAP_ERR_GET_FRAME;
					LOG("%s: failed to get frame %d(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
				} else {
					inputBuffer->pAddrVirY = (unsigned char*)cap_handle->priv_cap.vi_frame_info.pVirAddr[0];
					inputBuffer->pAddrVirC = (unsigned char*)cap_handle->priv_cap.vi_frame_info.pVirAddr[1];
					inputBuffer->pAddrPhyY = (unsigned char*)cap_handle->priv_cap.vi_frame_info.u32PhyAddr[0];
					inputBuffer->pAddrPhyC = (unsigned char*)cap_handle->priv_cap.vi_frame_info.u32PhyAddr[1];
					ret = EncoderStart(encode_param, inputBuffer, outputBuffer, VENCODE_CMD_STREAM);
					if (ret) {
						LOG("%s: detect video encode error!!@%d\n", __FUNCTION__, __LINE__);
						ret = CAP_ERR_VENCODE_STREAM;
					} else {
						//Encoder data save in outputBuffer
						cap_fmt->length = outputBuffer->nSize0 + outputBuffer->nSize1;
						memcpy(buffer, outputBuffer->pData0, outputBuffer->nSize0);
						if (outputBuffer->nSize1) {
							buffer += outputBuffer->nSize0;
							memcpy(buffer, outputBuffer->pData1, outputBuffer->nSize1);
						}
						//when we do the job with outputBuffer, should call this function to free it
						ret = EncoderFreeOutputBuffer(&encode_param->outputBuffer);
						if (ret) {
							LOG("%s free video encode outputBuffer faile\n", __FUNCTION__);
							ret = CAP_ERR_VENCODE_FREEBUFFER;
						}
					}
					release_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info);
				}
				pthread_mutex_unlock(&cap_handle->locker);
			} else {
				LOG("%s: start_video fail\n", __FUNCTION__);
			}
			pthread_mutex_unlock(&g_cap_locker);
		} else if (type == SOCK_CMD_VENCODE_ENCPP_YUV) {
			pthread_mutex_lock(&g_cap_locker);
			cap_handle = g_cap_handle + cap_fmt->channel;
			ret = start_video(cap_handle, cap_fmt);
			if (CAP_ERR_NONE == ret) {
				pthread_mutex_lock(&cap_handle->locker);
				ret = get_video_frame(g_media_dev, cap_handle->priv_cap.vich,
									  &cap_handle->priv_cap.vi_frame_info,
									  cap_handle->priv_cap.timeout);
				cap_handle->frame_count++;
				if (ret < 0) {
					ret = CAP_ERR_GET_FRAME;
					LOG("%s: failed to get frame %d(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
				} else {
					inputBuffer->pAddrVirY = (unsigned char*)cap_handle->priv_cap.vi_frame_info.pVirAddr[0];
					inputBuffer->pAddrVirC = (unsigned char*)cap_handle->priv_cap.vi_frame_info.pVirAddr[1];
					inputBuffer->pAddrPhyY = (unsigned char*)cap_handle->priv_cap.vi_frame_info.u32PhyAddr[0];
					inputBuffer->pAddrPhyC = (unsigned char*)cap_handle->priv_cap.vi_frame_info.u32PhyAddr[1];
					ret = EncoderStart(encode_param, inputBuffer, outputBuffer, VENCODE_CMD_STREAM);
					if (ret) {
						LOG("%s: detect video encode error!!@%d\n", __FUNCTION__, __LINE__);
						ret = CAP_ERR_VENCODE_STREAM;
					} else {
						if (cap_fmt->height % 16 == 0) {
							cap_fmt->length = cap_fmt->width * cap_fmt->height * 3 / 2;
						} else {
							cap_fmt->length = cap_fmt->width * ((cap_fmt->height / 16) + 1) * 16 * 3 / 2;
						}
						EncoderGetWbYuv(encode_param, buffer, cap_fmt->length);
						if(cap_fmt->height % 16) { // height ALIGN
							uptr = cap_fmt->buffer + (cap_fmt->width * cap_fmt->height);
							for(i = 0; i < cap_fmt->width * cap_fmt->height / 2; i++) {
								*(uptr + i) = *(uptr + i + (cap_fmt->width * (16 - cap_fmt->height % 16)));
							}
						}
						//when we do the job with outputBuffer, should call this function to free it
						ret = EncoderFreeOutputBuffer(&encode_param->outputBuffer);
						if (ret) {
							LOG("%s free video encode outputBuffer faile\n", __FUNCTION__);
							ret = CAP_ERR_VENCODE_FREEBUFFER;
						}
					}
					release_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info);
				}
				pthread_mutex_unlock(&cap_handle->locker);
			} else {
				LOG("%s: start_video fail\n", __FUNCTION__);
			}
			pthread_mutex_unlock(&g_cap_locker);
		}
	} else {
		ret = CAP_ERR_INVALID_PARAMS;
	}

	return ret;

}
#endif

int get_capture_buffer_transfer(capture_format *cap_fmt)
{
	int ret = CAP_ERR_NONE;
	unsigned char *buffer = NULL;
	capture_params *cap_handle = NULL;
	int i = 0;
	unsigned char *uptr = NULL;

	if (cap_fmt && VALID_VIDEO_SEL(cap_fmt->channel)) {
		pthread_mutex_lock(&g_cap_locker);
		cap_handle = g_cap_handle + cap_fmt->channel;
		//LOG("%s: %d\n", __FUNCTION__, __LINE__);
		ret = start_video(cap_handle, cap_fmt);
		//LOG("%s: %d\n", __FUNCTION__, __LINE__);
		if (CAP_ERR_NONE == ret) {
			// get frame
			//LOG("%s: ready to get frame(channel %d)\n", __FUNCTION__, cap_fmt->channel);
			//LOG("%s: %d\n", __FUNCTION__, __LINE__);
			pthread_mutex_lock(&cap_handle->locker);
			//LOG("%s: %d\n", __FUNCTION__, __LINE__);
			cap_fmt->length = 0;
			buffer = cap_fmt->buffer;
			for(i=0; i<cap_fmt->framecount; i++){//save cap_fmt->framecount frames
				ret = get_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info, cap_handle->priv_cap.timeout);
				//LOG("%s: get frame %d done(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
				cap_handle->frame_count++;
				if (ret < 0) {
					ret = CAP_ERR_GET_FRAME;
					LOG("%s: failed to get frame %d(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
				} else {
					if (cap_handle->frame_count % 50 == 0) {
						LOG("%s: get frame %d done(channel %d)\n", __FUNCTION__, cap_handle->frame_count, cap_fmt->channel);
					}
					if( (cap_fmt->length +  cap_fmt->width * cap_fmt->height*2) <(1 << 29)){
						for (ret = 0; ret < cap_fmt->planes_count; ret++) {
							memcpy(buffer, cap_handle->priv_cap.vi_frame_info.pVirAddr[ret],
								cap_handle->priv_cap.vi_frame_info.u32Stride[ret]);
							buffer += cap_handle->priv_cap.vi_frame_info.u32Stride[ret];
							cap_fmt->length += cap_handle->priv_cap.vi_frame_info.u32Stride[ret];
						}
#if (ISP_VERSION >= 521)
						if((cap_fmt->format != V4L2_PIX_FMT_SBGGR8) && (cap_fmt->format != V4L2_PIX_FMT_SGBRG8) &&
							(cap_fmt->format != V4L2_PIX_FMT_SGRBG8) && (cap_fmt->format != V4L2_PIX_FMT_SRGGB8) &&
							(cap_fmt->format != V4L2_PIX_FMT_SBGGR10) && (cap_fmt->format != V4L2_PIX_FMT_SGBRG10) &&
							(cap_fmt->format != V4L2_PIX_FMT_SGRBG10) && (cap_fmt->format != V4L2_PIX_FMT_SRGGB10) &&
							(cap_fmt->format != V4L2_PIX_FMT_SBGGR12) && (cap_fmt->format != V4L2_PIX_FMT_SGBRG12) &&
							(cap_fmt->format != V4L2_PIX_FMT_SGRBG12) && (cap_fmt->format != V4L2_PIX_FMT_SRGGB12) &&
							(cap_fmt->format != V4L2_PIX_FMT_LBC_2_0X) && (cap_fmt->format != V4L2_PIX_FMT_LBC_2_5X) &&
							(cap_fmt->format != V4L2_PIX_FMT_LBC_1_0X) && (cap_fmt->format != V4L2_PIX_FMT_LBC_1_5X)) {
							if(cap_fmt->height % 16) { // height ALIGN
								uptr = cap_fmt->buffer + (cap_fmt->width * cap_fmt->height);
								for(i = 0; i < cap_fmt->width * cap_fmt->height / 2; i++) {
									*(uptr + i) = *(uptr + i + (cap_fmt->width * (16 - cap_fmt->height % 16)));
								}
							}
						}
#endif
						//if (cap_fmt->length < cap_fmt->width * cap_fmt->height) {
						//	LOG("%s: %d < %dx%d, not matched\n", __FUNCTION__, cap_fmt->length, cap_fmt->width, cap_fmt->height);
						//	ret = CAP_ERR_GET_FRAME;
						//} else {
							ret = CAP_ERR_NONE;
						//}
					}
					release_video_frame(g_media_dev, cap_handle->priv_cap.vich, &cap_handle->priv_cap.vi_frame_info);
				}
			}
			pthread_mutex_unlock(&cap_handle->locker);
		}
		pthread_mutex_unlock(&g_cap_locker);
	} else {
		ret = CAP_ERR_INVALID_PARAMS;
	}

	return ret;
}

int start_raw_flow(capture_format *cap_fmt)
{
	int ret = CAP_ERR_NONE;
	capture_params *cap_handle = NULL;

	if (cap_fmt && VALID_VIDEO_SEL(cap_fmt->channel)) {	
		// check format
		if (!(cap_fmt->format == V4L2_PIX_FMT_SBGGR8 ||
			cap_fmt->format == V4L2_PIX_FMT_SGBRG8 ||
			cap_fmt->format == V4L2_PIX_FMT_SGRBG8 ||
			cap_fmt->format == V4L2_PIX_FMT_SRGGB8 ||
			cap_fmt->format == V4L2_PIX_FMT_SBGGR10 ||
			cap_fmt->format == V4L2_PIX_FMT_SGBRG10 ||
			cap_fmt->format == V4L2_PIX_FMT_SGRBG10 ||
			cap_fmt->format == V4L2_PIX_FMT_SRGGB10 ||
			cap_fmt->format == V4L2_PIX_FMT_SBGGR12 ||
			cap_fmt->format == V4L2_PIX_FMT_SGBRG12 ||
			cap_fmt->format == V4L2_PIX_FMT_SGRBG12 ||
			cap_fmt->format == V4L2_PIX_FMT_SRGGB12)) {
			LOG("%s: Not valid bayer format\n", __FUNCTION__);
			return CAP_ERR_INVALID_PARAMS;
		}

		// start channel
		pthread_mutex_lock(&g_cap_locker);
		cap_handle = g_cap_handle + cap_fmt->channel;
		ret = start_video(cap_handle, cap_fmt);
		if (CAP_ERR_NONE == ret) { // video on ok
			if (!(CAP_RAW_FLOW_RUNNING & cap_handle->status)) {
				// init raw flow
				ret = init_raw_flow(cap_fmt, CAPTURE_RAW_FLOW_QUEUE_SIZE);
				if (ret != ERR_RAW_FLOW_NONE) {
					ret = CAP_ERR_START_RAW_FLOW;
				} else {
					pthread_mutex_lock(&cap_handle->locker);
					cap_handle->status |= CAP_RAW_FLOW_START;
					pthread_mutex_unlock(&cap_handle->locker);
					do {
						msleep(1);
						pthread_mutex_lock(&cap_handle->locker);
						if (CAP_RAW_FLOW_RUNNING & cap_handle->status) {
							LOG("%s: vich%d done\n", __FUNCTION__, cap_handle->priv_cap.vich);
							pthread_mutex_unlock(&cap_handle->locker);
							break;
						}
						pthread_mutex_unlock(&cap_handle->locker);
					} while (1);
					ret = CAP_ERR_NONE;
				}
			} else {
				LOG("%s: raw flow is already running(vich%d)\n", __FUNCTION__, cap_handle->priv_cap.vich);
			}
		}
		pthread_mutex_unlock(&g_cap_locker);
	} else {
		ret = CAP_ERR_INVALID_PARAMS;
	}
	return ret;
}

int stop_raw_flow(int channel)
{
	capture_params *cap_handle = NULL;
	if (!VALID_VIDEO_SEL(channel)) {
		return CAP_ERR_INVALID_PARAMS;
	}
	
	pthread_mutex_lock(&g_cap_locker);
	cap_handle = g_cap_handle + channel;
	pthread_mutex_lock(&cap_handle->locker);
	cap_handle->status &= ~CAP_RAW_FLOW_START;
	pthread_mutex_unlock(&cap_handle->locker);
	do {
		msleep(1);
		pthread_mutex_lock(&cap_handle->locker);
		if (!(CAP_RAW_FLOW_RUNNING & cap_handle->status)) {
			LOG("%s: vich%d done\n", __FUNCTION__, cap_handle->priv_cap.vich);
			pthread_mutex_unlock(&cap_handle->locker);
			break;
		}
		pthread_mutex_unlock(&cap_handle->locker);
	} while (1);
	pthread_mutex_unlock(&g_cap_locker);
	
	if (exit_raw_flow() != ERR_RAW_FLOW_NONE) {
		return CAP_ERR_STOP_RAW_FLOW;
	} else {
		return CAP_ERR_NONE;
	}
}

int get_raw_flow_frame(capture_format *cap_fmt)
{
	capture_params *cap_handle = NULL;
	if (!cap_fmt || !VALID_VIDEO_SEL(cap_fmt->channel)) {
		return CAP_ERR_INVALID_PARAMS;
	}

	pthread_mutex_lock(&g_cap_locker);
	cap_handle = g_cap_handle + cap_fmt->channel;
	pthread_mutex_lock(&cap_handle->locker);
	if (!(CAP_RAW_FLOW_RUNNING & cap_handle->status)) {
		pthread_mutex_unlock(&cap_handle->locker);
		pthread_mutex_unlock(&g_cap_locker);
		return CAP_ERR_RAW_FLOW_NOT_RUN;
	}
	pthread_mutex_unlock(&cap_handle->locker);
	pthread_mutex_unlock(&g_cap_locker);

	if (dequeue_raw_flow(cap_fmt) != ERR_RAW_FLOW_NONE) {
		return CAP_ERR_GET_RAW_FLOW;
	} else {
		return CAP_ERR_NONE;
	}
}

void *do_save_raw_flow(void *params)
{
	FILE *fp = NULL;
	capture_format cap_fmt;
	int ret = 0, frames_count = 0;
	
	if (params) {
		fp = (FILE *)params;
		cap_fmt.buffer = (unsigned char *)malloc(1 << 24); // 16M
		do {
			ret = dequeue_raw_flow(&cap_fmt);
			if (ERR_RAW_FLOW_NONE == ret) {
				LOG("%s: %d\n", __FUNCTION__, __LINE__);
				fwrite(cap_fmt.buffer, cap_fmt.length, 1, fp);
				LOG("%s: %d\n", __FUNCTION__, __LINE__);
				//fflush(fp);
				frames_count++;
			} else {
				if (ERR_RAW_FLOW_QUEUE_EMPTY == ret) {
					continue;
				} else {
					break;
				}
			}
		} while (1);
		fclose(fp);
		fp = NULL;
		free(cap_fmt.buffer);
		cap_fmt.buffer = NULL;
		LOG("%s: save done(frames %d)\n", __FUNCTION__, frames_count);
	}
	return 0;
}

void save_raw_flow(const char *file_name)
{
	FILE *fp = NULL;

	if (file_name) {
		fp = fopen(file_name, "wb");
		if (fp) {
			add_work(&do_save_raw_flow, fp);
		} else {
			LOG("%s: failed to open %s\n", __FUNCTION__, file_name);
		}
	}
}



