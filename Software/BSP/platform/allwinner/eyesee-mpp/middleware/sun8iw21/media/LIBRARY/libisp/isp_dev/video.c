
/*
 * zw
 * for csi & isp test
 */
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>

#include <assert.h>
#include <sys/mman.h>

#include <sys/ioctl.h>
#include <sys/time.h>

#include "../include/device/video.h"
#include "isp_v4l2_helper.h"
#include "../include/device/isp_dev.h"

#define SUNXI_VIDEO "vin_video"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#define CHECK_SOC_FT_ZONE 5

int check_soc_ft_zone(int csi_id)
{
	int fd;
	char buf[9];
	int ret = 0;
	int value = 0;

	fd = open("/dev/sunxi_soc_info", O_RDONLY);
	if (fd < 0){
		printf("open /dev/sunxi_soc_info failed!");
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	ret = ioctl(fd, CHECK_SOC_FT_ZONE, buf);
	close(fd);

	if (ret < 0) {
		printf("ioctl err!\n");
		return ret;
	}

	//value = atoi(buf);
	sscanf(buf, "%x", &value);
	ISP_PRINT("chipid: 0x%x!!!\n", value);
	if ((value & 0x10) && csi_id == 1)
		return -1;
	else
		return 0;
}

int check_soc_ft_fps(int *frame_mode)
{
	int fd;
	char buf[9];
	int ret = 0;
	int value = 0;

	fd = open("/dev/sunxi_soc_info", O_RDONLY);
	if (fd < 0){
		printf("open /dev/sunxi_soc_info failed!");
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	ret = ioctl(fd, CHECK_SOC_FT_ZONE, buf);
	close(fd);

	if (ret < 0) {
		printf("ioctl err!\n");
		return ret;
	}

	//value = atoi(buf);
	sscanf(buf, "%x", &value);
	if (value & 0x20) {
		*frame_mode = 1;
		ISP_PRINT("Size cantnot exceed 4K25fps!!!\n");
	} else
		*frame_mode = 0;

    return 0;
}

int video_init(struct isp_video_device *video)
{
	struct hw_isp_media_dev *media = video->priv;
	char name[32];

	snprintf(name, sizeof(name), SUNXI_VIDEO"%d", (int)video->id);
	ISP_PRINT("video device name is %s\n", name);
	video->entity = media_get_entity_by_name(media->mdev, name);
	if (video->entity == NULL) {
		ISP_ERR("can not get entity by name %s\n", name);
		return -ENOENT;
	}

	if (video->entity->fd != -1)
		return 0;

	video->entity->fd = open(video->entity->devname, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
	if (video->entity->fd == -1) {
		ISP_ERR("%s: Failed to open subdev device node %s\n", __func__,
			video->entity->devname);
		return -errno;
	}
	return 0;
}

void video_cleanup(struct isp_video_device *video)
{
	if (video->entity->fd == -1)
		return;

	close(video->entity->fd);
	video->entity->fd = -1;
}

int video_to_isp_id(struct isp_video_device *video)
{
	return video->isp_id;
}

int video_set_fmt(struct isp_video_device *video, struct video_fmt *vfmt)
{
	struct v4l2_format fmt;
	struct v4l2_input inp;
	struct v4l2_streamparm parms;
	struct sensor_isp_cfg sensor_isp_cfg;
	int frame_mode;
	struct csi_ve_online_cfg ve_online_cfg;

	memset(&inp, 0, sizeof inp);
	inp.index = vfmt->index;
	if (-1 == ioctl(video->entity->fd, VIDIOC_S_INPUT, &inp)) {
		ISP_ERR("VIDIOC_S_INPUT %d error!\n", 0);
		return -1;
	}

	// only vipp0 support online
	if (0 == video->id)
	{
		memset(&ve_online_cfg, 0, sizeof ve_online_cfg);
		ve_online_cfg.ve_online_en = vfmt->ve_online_en;
		ve_online_cfg.dma_buf_num  = vfmt->dma_buf_num;
		ISP_PRINT("video%d fd[%d] ve_online_en=%d, dma_buf_num=%d\n",
			video->id, video->entity->fd, ve_online_cfg.ve_online_en, ve_online_cfg.dma_buf_num);
		/* must set after VIDIOC_S_INPUT and before VIDIOC_S_PARM */
		if (-1 == ioctl(video->entity->fd, VIDIOC_SET_VE_ONLINE, &ve_online_cfg)) {
			ISP_ERR("video fd[%d] VIDIOC_SET_VE_ONLINE error\n", video->entity->fd);
			return -1;
		}
	}

	memset(&parms, 0, sizeof parms);
	parms.type = vfmt->type;
	parms.parm.capture.timeperframe.numerator = 1;
	parms.parm.capture.timeperframe.denominator = vfmt->fps; // 30;
	parms.parm.capture.capturemode = vfmt->capturemode;
	parms.parm.capture.reserved[0] = vfmt->use_current_win;/*when different video have the same sensor source, 1:use sensor current win, 0:find the nearest win*/
	parms.parm.capture.reserved[1] = vfmt->wdr_mode; /*2:comanding, 1: wdr, 0: normal*/

	if (-1 == ioctl(video->entity->fd, VIDIOC_S_PARM, &parms)) {
		ISP_ERR("VIDIOC_S_PARM error\n");
		return -1;
	}

	memset(&sensor_isp_cfg, 0, sizeof sensor_isp_cfg);
	sensor_isp_cfg.isp_wdr_mode = vfmt->wdr_mode;/*2:command, 1: wdr, 0: normal*/
	if (-1 == ioctl(video->entity->fd, VIDIOC_SET_SENSOR_ISP_CFG, &sensor_isp_cfg)) {
		ISP_ERR("VIDIOC_SET_SENSOR_ISP_CFG error\n");
	}
	memset(&fmt, 0, sizeof fmt);
	fmt.type = vfmt->type;
	fmt.fmt.pix_mp = vfmt->format;

	if (-1 == ioctl(video->entity->fd, VIDIOC_S_FMT, &fmt)) {
		ISP_ERR("VIDIOC_S_FMT error!\n");
		return -1;
	}

	if (-1 == ioctl(video->entity->fd, VIDIOC_G_FMT, &fmt)) {
		ISP_ERR("VIDIOC_G_FMT error!\n");
		return -1;
	} else {
		video->nplanes = fmt.fmt.pix_mp.num_planes;
		video->format = fmt.fmt.pix_mp;
		ISP_DEV_LOG(ISP_LOG_VIDEO, "get resolution: %d*%d num_planes: %d\n",
		       fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
		       fmt.fmt.pix_mp.num_planes);
	}

	if (-1 == ioctl(video->entity->fd, VIDIOC_G_PARM, &parms)) {
		ISP_ERR("VIDIOC_G_PARM error\n");
		return -1;
	}
	vfmt->capturemode = parms.parm.capture.capturemode;

	check_soc_ft_fps(&frame_mode);
	if (frame_mode) {
		if (fmt.fmt.pix_mp.width >= 3840 && fmt.fmt.pix_mp.height >= 2160 &&
			parms.parm.capture.timeperframe.denominator > 25) {
			ISP_ERR("Not support size that exceed 4K25fps!!!\n");
			return -1;
		}
	}

	video->type = vfmt->type;
	video->memtype = vfmt->memtype;
	video->capturemode = vfmt->capturemode;
	video->use_current_win = vfmt->use_current_win;
	video->wdr_mode = vfmt->wdr_mode;
	video->nbufs = vfmt->nbufs;
	video->fps = vfmt->fps;
	video->drop_frame_num = vfmt->drop_frame_num;
	video->ve_online_en = vfmt->ve_online_en;
	video->dma_buf_num = vfmt->dma_buf_num;

	struct tdm_speeddn_cfg speeddn_cfg;
	memset(&speeddn_cfg, 0, sizeof speeddn_cfg);
	speeddn_cfg.pix_num = vfmt->pixel_num;
	speeddn_cfg.tdm_speed_down_en = vfmt->tdm_speed_down_en;
	speeddn_cfg.tdm_tx_valid_num = 1;
	speeddn_cfg.tdm_tx_invalid_num = 0;
	if (-1 == ioctl(video->entity->fd, VIDIOC_SET_TDM_SPEEDDN_CFG, &speeddn_cfg)) {
		ISP_ERR("VIDIOC_SET_TDM_SPEEDDN_CFG error!\n");
		return -1;
	}

	if (vfmt->video_selection_en) {
		struct v4l2_selection s;
		memset(&s, 0, sizeof s);
		s.target = V4L2_SEL_TGT_CROP;
		s.r.left = vfmt->rect.left;
		s.r.top = vfmt->rect.top;
		s.r.width = vfmt->rect.width;
		s.r.height = vfmt->rect.height;
		if (-1 == ioctl(video->entity->fd, VIDIOC_S_SELECTION, &s)) {
			ISP_ERR("video%d VIDIOC_S_SELECTION failed\n", video->id);
			return -1;
		}
	}

	return 0;
}

int video_get_fmt(struct isp_video_device *video, struct video_fmt *vfmt)
{
	vfmt->type = video->type;
	vfmt->memtype = video->memtype;
	vfmt->format = video->format;
	vfmt->capturemode = video->capturemode;
	vfmt->use_current_win = video->use_current_win;
	vfmt->wdr_mode = video->wdr_mode;
	vfmt->nbufs = video->nbufs;
	vfmt->nplanes = video->nplanes;
	vfmt->fps = video->fps;
	vfmt->drop_frame_num = video->drop_frame_num;
	vfmt->ve_online_en = video->ve_online_en;
	vfmt->dma_buf_num = video->dma_buf_num;

	return 0;
}

int video_req_buffers(struct isp_video_device *video, struct buffers_pool *pool)
{
	struct v4l2_requestbuffers rb;
	struct v4l2_buffer buf;
	unsigned int i, j;
	int ret;

	if ((video->nplanes > VIDEO_MAX_PLANES) || (video->nplanes <= 0)) {
		printf("planes number is error!\n");
		return -1;
	}

	video->pool = pool;

	memset(&rb, 0, sizeof rb);
	rb.count = pool->nbufs;
	rb.type = video->type;
	rb.memory = video->memtype;

	ret = ioctl(video->entity->fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		ISP_ERR("%s: unable to request buffers (%d).\n", video->entity->devname,
		       errno);
		goto done;
	}

	if (rb.count > pool->nbufs) {
		ISP_ERR("%s: driver needs more buffers (%u) than available (%u).\n",
		       video->entity->devname, rb.count, pool->nbufs);
		goto done;
	}
	video->nbufs = rb.count;
	ISP_DEV_LOG(ISP_LOG_VIDEO, "%s: %u buffers requested.\n",
			video->entity->devname, rb.count);

	/* Map the buffers. */
	for (i = 0; i < rb.count; ++i) {
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		memset(&buf, 0, sizeof buf);
		memset(planes, 0, sizeof planes);

		buf.type = video->type;
		buf.memory = video->memtype;
		buf.index = i;
		buf.length = video->nplanes;
		buf.m.planes = planes;
		ret = ioctl(video->entity->fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0) {
			ISP_ERR("%s: unable to query buffer %u (%d).\n",
			       video->entity->devname, i, errno);
			free(buf.m.planes);
			goto done;
		}
		switch (video->memtype) {
		case V4L2_MEMORY_MMAP:
			for (j = 0; j < video->nplanes; j++) {
				pool->buffers[i].planes[j].size = buf.m.planes[j].length;
				pool->buffers[i].planes[j].mem =
				    mmap(NULL /* start anywhere */ ,
					 buf.m.planes[j].length,
					 PROT_READ | PROT_WRITE /* required */ ,
					 MAP_SHARED /* recommended */ ,
					 video->entity->fd, buf.m.planes[j].m.mem_offset);

				if (MAP_FAILED == pool->buffers[i].planes[j].mem) {
					printf("%s: unable to map buffer %u (%d)\n",
					       video->entity->devname, i, errno);
					goto done;
				}
				ISP_DEV_LOG(ISP_LOG_VIDEO, "%s: buffer %u planes %d mapped at address %p\n",
					video->entity->devname, i, (int)j,
					pool->buffers[i].planes[j].mem);
			}
			break;
#if 0
		case V4L2_MEMORY_USERPTR: {
			struct video_buffer *buffer = &pool->buffers[i];
			for (j = 0; j < buffer->nplanes; ++j) {
				struct video_plane *plane = &buffer->planes[j];
				plane->size = buf.m.planes[j].length;
				plane->mem = (void *)ion_alloc(plane->size);
				ISP_DEV_LOG(ISP_LOG_VIDEO, "%s: buffer %u plane %d size %d addr %p\n",
					video->entity->devname, i, j, plane->size, plane->mem);
				if (plane->mem == NULL) {
					ISP_ERR("%s: plane %d alloc buf failed!\n",
						video->entity->devname, j);
					goto done;
				}
			}
			buffer->allocated = true;
			break;
		}
		case V4L2_MEMORY_DMABUF: {
			struct video_buffer *buffer = &pool->buffers[i];
			for (j = 0; j < buffer->nplanes; ++j) {
				struct video_plane *plane = &buffer->planes[j];
				plane->size = buf.m.planes[j].length;
				plane->mem = (void *)ion_alloc(plane->size);
				plane->dma_fd = ion_vir2fd(plane->mem);
				ISP_DEV_LOG(ISP_LOG_VIDEO, "%s: buffer %u plane %d size %d addr %p dma_fd %d\n",
					video->entity->devname, i, j, plane->size,
					plane->mem, plane->dma_fd);
				if (plane->mem == NULL) {
					ISP_ERR("%s: plane %d alloc buf failed!\n",
						video->entity->devname, j);
					goto done;
				}
			}
			buffer->allocated = true;
			break;
		}
#endif
		default:
			break;
		}
	}
	return 0;
done:
	video_free_buffers(video);
	return -1;
}

int video_free_buffers(struct isp_video_device *video)
{
	struct v4l2_requestbuffers rb;
	unsigned int i, j;
	int ret;

	if (video->pool == NULL)
		return 0;

	switch (video->memtype) {
	case V4L2_MEMORY_MMAP:
		for (i = 0; i < video->nbufs; ++i) {
			struct video_buffer *buffer = &video->pool->buffers[i];
			for (j = 0; j < video->nplanes; j++) {
				struct video_plane *plane = &buffer->planes[j];

				if (plane->mem == NULL)
					continue;

				if (munmap(plane->mem, plane->size)) {
					printf("%s: unable to unmap buffer %u (%d)\n",
						video->entity->devname, i, errno);
					return -errno;
				}
				plane->mem = NULL;
				plane->size = 0;
			}
		}
		break;
#if 0
	case V4L2_MEMORY_USERPTR:
	case V4L2_MEMORY_DMABUF:
		for (i = 0; i < dev->nbufs; ++i) {
			struct video_buffer *buffer = &dev->pool->buffers[i];
			for (j = 0; j < dev->nplanes; j++) {
				struct video_plane *plane = &buffer->planes[j];
				if (NULL != plane->mem)
					ion_free(plane->mem);
				plane->mem = NULL;
				plane->size = 0;
			}
		}
		break;
#endif
	default:
		break;
	}

	memset(&rb, 0, sizeof rb);
	rb.count = 0;
	rb.type = video->type;
	rb.memory = video->memtype;

	ret = ioctl(video->entity->fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		ISP_ERR("%s: unable to release buffers (%d)\n",
			video->entity->devname, errno);
		return -errno;
	}

	video->nbufs = 0;
	video->nplanes = 0;

	return 0;
}

int video_wait_buffer(struct isp_video_device *video, int timeout)
{
	fd_set fds;
	struct timeval tv;
	int r;

	FD_ZERO(&fds);
	FD_SET(video->entity->fd, &fds);

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	r = select(video->entity->fd + 1, &fds, NULL, NULL, &tv);

	if (-1 == r) {
		ISP_ERR("video%d select error!\n", (int)video->id);
		return -1;
	}
	if (0 == r) {
		ISP_ERR("video%d select timeout!\n", (int)video->id);
		return -1;
	}
	return 0;
}

int video_dequeue_buffer(struct isp_video_device *video,
			struct video_buffer *buffer)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	int i = 0, ret = 0;

	if ((video->nplanes > VIDEO_MAX_PLANES) || (video->nplanes <= 0)) {
		ISP_ERR("planes number is error!\n");
		return -1;
	}

	memset(&buf, 0, sizeof buf);
	memset(planes, 0, sizeof planes);

	buf.type = video->type;
	buf.memory = video->memtype;
	buf.length = video->nplanes;
	buf.m.planes = planes;
	ret = ioctl(video->entity->fd, VIDIOC_DQBUF, &buf);
	if (ret < 0) {
		ISP_ERR("%s: unable to dequeue buffer index %u/%u (%d)\n",
		       video->entity->devname, buf.index, video->nbufs, errno);
		return -1;
	}

	assert(buf.index < video->nbufs);

	*buffer = video->pool->buffers[buf.index];
	buffer->bytesused = buf.bytesused;
	buffer->frame_cnt = buf.reserved;
	buffer->exp_time = buf.reserved2;
	buffer->timestamp = buf.timestamp;
	buffer->error = !!(buf.flags & V4L2_BUF_FLAG_ERROR);
	for (i = 0; i < video->nplanes; i++) {
		struct video_plane *plane = &buffer->planes[i];
		plane->mem_phy = buf.m.planes[i].m.mem_offset;
	}
	return 0;
}

int video_queue_buffer(struct isp_video_device *video, unsigned int buf_id)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct video_buffer *buffer = &video->pool->buffers[buf_id];
	int ret;
	unsigned int j;

	if (buffer->index >= video->nbufs)
		return -1;

	if ((video->nplanes > VIDEO_MAX_PLANES) || (video->nplanes <= 0)) {
		ISP_ERR("planes number is error!\n");
		return -1;
	}

	memset(&buf, 0, sizeof buf);
	memset(planes, 0, sizeof planes);

	buf.index = buffer->index;
	buf.type = video->type;
	buf.memory = video->memtype;
	buf.length = video->nplanes;
	buf.m.planes = planes;
	switch (video->memtype) {
	case V4L2_MEMORY_USERPTR:
		for (j = 0; j < video->nplanes; j++) {
			struct video_plane *plane = &buffer->planes[j];
			buf.m.planes[j].m.userptr = (unsigned long)plane->mem;
			buf.m.planes[j].length = plane->size;
		}
		break;
	case V4L2_MEMORY_DMABUF:
		for (j = 0; j < video->nplanes; j++) {
			struct video_plane *plane = &buffer->planes[j];
			buf.m.planes[j].m.fd = plane->dma_fd;
		}
		break;
	default:
		break;
	}

	if (video->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		buf.bytesused = buffer->bytesused;

	ret = ioctl(video->entity->fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		ISP_ERR("%s: unable to queue buffer index %u/%u (%d)\n",
		       video->entity->devname, buf.index, video->nbufs, errno);
		return -1;
	}

	return 0;
}

int video_stream_on(struct isp_video_device *video)
{
	int type = video->type;
	int ret;

	ret = ioctl(video->entity->fd, VIDIOC_STREAMON, &type);
	if (ret < 0)
		return -errno;

	return 0;
}

int video_stream_off(struct isp_video_device *video)
{
	int type = video->type;
	int ret;

	ret = ioctl(video->entity->fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0)
		return -errno;

	return 0;
}

struct buffers_pool *buffers_pool_new(struct isp_video_device *video)
{
	struct video_plane *planes;
	struct video_buffer *buffers;
	struct buffers_pool *pool;
	unsigned int i;
	unsigned int nbufs = video->nbufs;
	unsigned int nplanes = video->nplanes;
	ISP_DEV_LOG(ISP_LOG_VIDEO, "creat buf pool, nbufs %d, nplanes %d!\n",
		(int)nbufs, (int)nplanes);

	pool = calloc(1, sizeof *pool);
	if (pool == NULL) {
		ISP_ERR("pool struct calloc failed!\n");
		return NULL;
	}

	buffers = calloc(nbufs, sizeof *buffers);
	if (buffers == NULL) {
		free(pool);
		ISP_ERR("buffers struct calloc failed!\n");
		return NULL;
	}

	for (i = 0; i < nbufs; ++i) {
		planes = calloc(nplanes, sizeof *planes);
		if (planes == NULL) {
			free(buffers);
			free(pool);
			ISP_ERR("planes struct calloc failed!\n");
			return NULL;
		}
		buffers[i].index = i;
		buffers[i].planes = planes;
		buffers[i].nplanes = nplanes;
	}

	pool->nbufs = nbufs;
	pool->buffers = buffers;

	//ion_alloc_open();

	return pool;
}

void buffers_pool_delete(struct isp_video_device *video)
{
	struct buffers_pool *pool = NULL;
	unsigned int i, j;

	if (video == NULL) {
		printf("%s error at %d\n", __func__, __LINE__);
		return;
	}
	pool = video->pool;
	if (pool == NULL) {
		printf("%s error at %d\n", __func__, __LINE__);
		return;
	}

	for (i = 0; i < pool->nbufs; ++i) {
		struct video_buffer *buffer = &pool->buffers[i];
		if (buffer->allocated) {
			for (j = 0; j < buffer->nplanes; ++j) {
				struct video_plane *plane = &buffer->planes[j];
				//if (plane->mem != NULL)
				//	ion_free(plane->mem);
				plane->mem = NULL;
				plane->size = 0;
			}
			buffer->nplanes = 0;
			free(buffer->planes);
			buffer->planes = NULL;
		}
	}
	pool->nbufs = 0;
	free(pool->buffers);
	pool->buffers = NULL;
	free(pool);
	video->pool = NULL;

	//ion_alloc_close();

}

int video_save_frames(struct isp_video_device *video, unsigned int buf_id, char *path)
{
	int j;
	FILE *file_fd = NULL;
	char fdstr[50];
	struct video_buffer *buffer = &video->pool->buffers[buf_id];

	for (j = 0; j < video->nplanes; j++) {
		//printf("file start = %p length = %d\n",
		//	buffer->planes[j].mem, buffer->planes[j].size);
		sprintf(fdstr, "%s/fb%d_yuv.bin", path, (int)video->id);
		file_fd = fopen(fdstr, "ab");
		fwrite(buffer->planes[j].mem, buffer->planes[j].size, 1, file_fd);
		fclose(file_fd);
	}
	return 0;
}

int overlay_set_fmt(struct isp_video_device *video, struct osd_fmt *ofmt)
{
	struct v4l2_format fmt;
	void *bitmap = NULL;
	unsigned int bitmap_size = 0, pix_size = 0;
	int i, ret = 0;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.clips = calloc(ofmt->clipcount * 2, sizeof(struct v4l2_clip));
	fmt.fmt.win.clipcount = ofmt->clipcount;
	fmt.fmt.win.chromakey = ofmt->chromakey;//V4L2_PIX_FMT_RGB32;
	fmt.fmt.win.field = V4L2_FIELD_NONE;//now use for reverse close;
	fmt.fmt.win.global_alpha = ofmt->global_alpha;

	if (ofmt->bitmap[0] != NULL) {
		for (i = 0; i < ofmt->clipcount; i++) {
			fmt.fmt.win.clips[i].c = ofmt->region[i];
			fmt.fmt.win.clips[i + ofmt->clipcount].c.top = ofmt->glb_alpha[i];
			fmt.fmt.win.clips[i + ofmt->clipcount].c.left = ofmt->reverse_close[i] + (ofmt->inv_th << 8);
			fmt.fmt.win.clips[i + ofmt->clipcount].c.width = ofmt->inv_w_rgn[i];
			fmt.fmt.win.clips[i + ofmt->clipcount].c.height = ofmt->inv_h_rgn[i];
			bitmap_size += ofmt->region[i].width * ofmt->region[i].height;
		}
		if (ofmt->clipcount) {
			switch (ofmt->chromakey) {
			case V4L2_PIX_FMT_RGB555:
				pix_size = 2;
				break;
			case V4L2_PIX_FMT_RGB444:
				pix_size = 2;
				break;
			case V4L2_PIX_FMT_RGB32:
				pix_size = 4;
				break;
			default:
				ISP_ERR("not support this chromakey\n");
				return -EINVAL;
			}
		} else {
			bitmap_size = 100;
			pix_size = 4;
		}

		bitmap = calloc(bitmap_size, pix_size);
		if (bitmap == NULL) {
			ISP_ERR("calloc of bitmap buf failed\n");
			return -ENOMEM;
		}
		fmt.fmt.win.bitmap = bitmap;
		for (i = 0; i < ofmt->clipcount; i++) {
			memcpy(bitmap, ofmt->bitmap[i], ofmt->region[i].width * ofmt->region[i].height * pix_size);
			bitmap += ofmt->region[i].width * ofmt->region[i].height * pix_size;
		}
	} else {
		fmt.fmt.win.bitmap = NULL;
		for (i = 0; i < ofmt->clipcount; i++) {
			fmt.fmt.win.clips[i].c = ofmt->region[i];
			fmt.fmt.win.clips[i + ofmt->clipcount].c.top = ofmt->rgb_cover[i];
		}
		if (ofmt->width)
			fmt.fmt.win.clips[ofmt->clipcount].c.width = ofmt->width;
	}

	ret = ioctl(video->entity->fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0)
		ISP_ERR("VIDIOC_S_FMT overlay return %d!\n", ret);

	free(fmt.fmt.win.clips);
	free(fmt.fmt.win.bitmap);

	return ret;
}

int overlay_update(struct isp_video_device *video, int on_off)
{
	int i;

	i = on_off;

	if (-1 == ioctl(video->entity->fd, VIDIOC_OVERLAY, &i)) {
		ISP_ERR("VIDIOC_OVERLAY error!\n");
		return errno;
	}
	return 0;
}

int orl_set_fmt(struct isp_video_device *video, struct orl_fmt *pOrlFmt)
{
	struct v4l2_format fmt;
	int i, ret = 0;
	memset(&fmt, 0, sizeof(fmt));

	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.field = V4L2_FIELD_NONE;//now use for reverse close;
	fmt.fmt.win.clips = calloc(pOrlFmt->clipcount * 2, sizeof(struct v4l2_clip));
	fmt.fmt.win.clipcount = pOrlFmt->clipcount;
	fmt.fmt.win.bitmap = NULL;
	fmt.fmt.win.global_alpha = 16;

	if(pOrlFmt->clipcount > 0) {
		for (i = 0; i < pOrlFmt->clipcount; i++) {
			fmt.fmt.win.clips[i].c = pOrlFmt->region[i];
			fmt.fmt.win.clips[i + pOrlFmt->clipcount].c.top = pOrlFmt->mRgbColor[i];
		}
		fmt.fmt.win.clips[pOrlFmt->clipcount].c.width = pOrlFmt->mThick;
	}

	ret = ioctl(video->entity->fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		ISP_ERR("fatal error! VIDIOC_S_FMT overlay return %d!\n", ret);
	}
	free(fmt.fmt.win.clips);

	if(fmt.fmt.win.bitmap != NULL) {
		free(fmt.fmt.win.bitmap);
	}
	return ret;
}

int video_set_control(struct isp_video_device *video, int cmd, int value)
{
	return v4l2_set_control(video->entity, cmd, &value);
}

int video_get_control(struct isp_video_device *video, int cmd, int *value)
{
	return v4l2_get_control(video->entity, cmd, value);
}

int video_query_control(struct isp_video_device *video, struct v4l2_queryctrl *ctrl)
{
	if (-1 == ioctl(video->entity->fd, VIDIOC_QUERYCTRL, ctrl)) {
		ISP_ERR("VIDIOC_S_CTRL error!\n");
		return errno;
	}
	return 0;
}

int video_query_menu(struct isp_video_device *video, struct v4l2_querymenu *menu)
{
	if (-1 == ioctl(video->entity->fd, VIDIOC_QUERYMENU, menu)) {
		ISP_ERR("VIDIOC_S_CTRL error!\n");
		return errno;
	}
	return 0;
}

int video_get_controls(struct isp_video_device *video, unsigned int count,
		      struct v4l2_ext_control *ctrls)
{
	return v4l2_get_controls(video->entity, count, ctrls);
}

int video_set_controls(struct isp_video_device *video, unsigned int count,
		      struct v4l2_ext_control *ctrls)
{
	return v4l2_set_controls(video->entity, count, ctrls);
}

int video_event_subscribe(struct isp_video_device *video, unsigned int type)
{
	struct v4l2_event_subscription sub;
	sub.type = type;
	sub.id = 0;
	sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL | V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
	if (-1 == ioctl (video->entity->fd, VIDIOC_SUBSCRIBE_EVENT, &sub)) {
		ISP_ERR("video%d subscribe event %d failed!\n", (int)video->id, (int)type);
		return -1;
	}
	return 0;
}

int video_event_unsubscribe(struct isp_video_device *video, unsigned int type)
{
	struct v4l2_event_subscription sub;
	sub.type = type;
	sub.id = 0;
	sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL | V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;;
	if (-1 == ioctl (video->entity->fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub)) {
		ISP_ERR("video%d unsubscribe event %d failed!\n", (int)video->id, (int)type);
		return -1;
	}
	return 0;
}

int video_wait_event(struct isp_video_device *video)
{
	fd_set efds;
	struct timeval tv;
	int r;

	FD_ZERO(&efds);
	FD_SET(video->entity->fd, &efds);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	r = select(video->entity->fd + 1, NULL, NULL, &efds, &tv);

	if (-1 == r) {
		ISP_ERR("video%d event select error!\n", (int)video->id);
		return -1;
	}
	if (0 == r) {
		ISP_ERR("video%d event select timeout!\n", (int)video->id);
		return -1;
	}
	return 0;
}

int video_dequeue_event(struct isp_video_device *video, struct video_event *vi_event)
{
	struct v4l2_event event;
	struct vin_vsync_event_data *data = (void *)event.u.data;


	if (-1 == ioctl (video->entity->fd, VIDIOC_DQEVENT, &event)) {
		ISP_ERR("video%d VIDIOC_DQEVENT error!\n", (int)video->id);
		return -1;
	}
	vi_event->event_id = event.id;
	vi_event->event_type = event.type;
	vi_event->vsync_ts = event.timestamp;
	vi_event->frame_cnt = data->frame_number;

	//ISP_PRINT("video%d VIDIOC_DQEVENT, time %d, frame count %d!\n",
	//	video->id, vi_event->vsync_ts.tv_sec, vi_event->frame_cnt);

	return 0;
}

int video_set_top_clk(struct isp_video_device *video, unsigned int rate)
{
	struct vin_top_clk clk;

	clk.clk_rate = rate;

	if (-1 == ioctl(video->entity->fd, VIDIOC_SET_TOP_CLK, &clk)) {
		ISP_ERR("video%d VIDIOC_SET_TOP_CLK failed\n", (int)video->id);
		return -1;
	}
	return 0;
}

int video_set_selection(struct isp_video_device *video, struct video_selection_rect *rect)
{
	struct v4l2_selection s;

	memset(&s, 0, sizeof s);
	s.target = V4L2_SEL_TGT_CROP;
	s.r.left = rect->left;
	s.r.top = rect->top;
	s.r.width = rect->width;
	s.r.height = rect->height;
	if (-1 == ioctl(video->entity->fd, VIDIOC_S_SELECTION, &s)) {
		ISP_ERR("video%d VIDIOC_S_SELECTION failed\n", video->id);
		return -1;
	}

	return 0;
}
