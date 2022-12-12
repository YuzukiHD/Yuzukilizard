/*
 * CameraV4l2.cpp
 *
 *  Created on: 2016骞�6鏈�30鏃�
 *      Author: A
 */

#include "CameraV4l2.h"

extern "C" {
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <unistd.h>
}
namespace BYL {

CameraV4l2::CameraV4l2() :
		_captureWidth(320), _captureHeight(240),
		_captureFormat(V4L2_PIX_FMT_MJPEG), _bufferTotal(0),
		_frameBufferArray(NULL), _cameraFd(-1) {
	// TODO Auto-generated constructor stub

}

int CameraV4l2::openDevice(const string &name) {
	if ((_cameraFd = open(name.c_str(), O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		return -1;
	}

	if (-1 == configureCamera()) {
		perror("Fail to configure the camera!");
		return -1;
	}

	return 0;
}

int CameraV4l2::getCaptureData(unsigned char *&data, int &size) {
	size = _captureWidth * _captureHeight * 3;
	data = new unsigned char[size];
	// TODO : mmap and memcpy
//	data = new unsigned char[_frameBufferArray[0].length];
	while (read_frame(data, size))
		;
}

void CameraV4l2::setCaptureSize(int width, int height) {
	_captureWidth = width;
	_captureHeight = height;
}

void CameraV4l2::setCaptureFormat(unsigned int type) {
	_captureFormat = type;
}

int CameraV4l2::freeCaptureData(unsigned char* &data) {
	if (data != NULL) {
		free(data);
		data = NULL;
	}
}

int CameraV4l2::saveFrameData(const string &fileName) {
	printf("write to file %s start!\n", fileName.c_str());
	FILE* fd = fopen(fileName.c_str(), "w");
	if (NULL == fd) {
		printf("open %s file failed!\n", fileName.c_str());
		return -1;
	}
	unsigned char *data = NULL;
	int length = 0;
	if (-1 == getCaptureData(data, length)) {
		freeCaptureData(data);
		return -1;
	}
	printf("write data to file :%s \nlenght: %d\n", fileName.c_str(), length);
//	if (V4L2_PIX_FMT_YUYV == _captureFormat) {
//		fwrite("P6\n320 240\n255\n", sizeof("P6\n320 240\n255\n"), 1, fd);
//	}
	fwrite(data, length, 1, fd);
	freeCaptureData(data);
	fclose(fd);
}

CameraV4l2::~CameraV4l2() {
	// TODO Auto-generated destructor stub
	closeDevice();
}

int CameraV4l2::closeDevice() {
	stop_capturing();
}

int CameraV4l2::configureCamera() {
	do {
		if (0 != init_camera_device())
			break;
		if (0 != start_capturing())
			break;
		return 0;
	} while (0);
	return -1;
}

int CameraV4l2::init_camera_device() {
	struct v4l2_fmtdesc fmt;
	struct v4l2_capability cap;
	struct v4l2_format stream_fmt;
	struct v4l2_streamparm stream_parm;
	int ret;

	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	while ((ret = ioctl(_cameraFd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
		fmt.index++;

		printf("{pixelformat = %c%c%c%c},description = '%s'\n",
				fmt.pixelformat & 0xff, (fmt.pixelformat >> 8) & 0xff,
				(fmt.pixelformat >> 16) & 0xff, (fmt.pixelformat >> 24) & 0xff,
				fmt.description);
	}

	ret = ioctl(_cameraFd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		perror("FAIL to ioctl VIDIOC_QUERYCAP");
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		printf("The Current device is not a video capture device\n");
		exit(EXIT_FAILURE);

	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		printf("The Current device does not support streaming i/o\n");
		exit(EXIT_FAILURE);
	}

	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stream_fmt.fmt.pix.width = _captureWidth;
	stream_fmt.fmt.pix.height = _captureHeight;
	stream_fmt.fmt.pix.pixelformat = _captureFormat;
	stream_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (-1 == ioctl(_cameraFd, VIDIOC_S_FMT, &stream_fmt)) {
		perror("Fail to ioctl");
		exit(EXIT_FAILURE);
	}

	memset(&stream_parm, 0, sizeof(stream_parm));
	stream_parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stream_parm.parm.capture.timeperframe.numerator = 1;
	stream_parm.parm.capture.timeperframe.denominator = 10;
	if (-1 == ioctl(_cameraFd, VIDIOC_S_PARM, &stream_parm)) {
		printf("VIDIOC_S_PARM failed!\n");
		exit(-1);
	}
	printf("numerator: %d, denominator: %d\n",
			stream_parm.parm.capture.timeperframe.numerator,
			stream_parm.parm.capture.timeperframe.denominator);

	if (init_mmap()) {
		printf("init_mmap failed!\n");
		return -1;
	}

	return 0;
}

int CameraV4l2::init_mmap() {
	unsigned int i = 0;
	struct v4l2_requestbuffers reqbuf;

	bzero(&reqbuf, sizeof(reqbuf));
	reqbuf.count = 8;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;

	if (-1 == ioctl(_cameraFd, VIDIOC_REQBUFS, &reqbuf)) {
		perror("Fail to ioctl 'VIDIOC_REQBUFS'");
		exit(EXIT_FAILURE);
	}
	_bufferTotal = reqbuf.count;
	printf("n_buffer = %d\n", _bufferTotal);

	_frameBufferArray = (FrameBuffer*) calloc(reqbuf.count,
			sizeof(FrameBuffer));
	if (!(_frameBufferArray)) {
		printf("calloc the Frame_buf struct buf failed!\n");
		return -1;
	}

	for (i = 0; i < reqbuf.count; i++) {
		struct v4l2_buffer buf;

		bzero(&buf, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == ioctl(_cameraFd, VIDIOC_QUERYBUF, &buf)) {
			perror("Fail to ioctl : VIDIOC_QUERYBUF");
			exit(EXIT_FAILURE);
		}

		_frameBufferArray[i].length = buf.length;
		_frameBufferArray[i].start = (unsigned char*) mmap(
		NULL,/*start anywhere*/
		buf.length,
		PROT_READ | PROT_WRITE,
		MAP_SHARED, _cameraFd, buf.m.offset);
		if (MAP_FAILED == _frameBufferArray[i].start) {
			perror("Fail to mmap");
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

int CameraV4l2::start_capturing() {
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < _bufferTotal; i++) {
		struct v4l2_buffer buf;

		bzero(&buf, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == ioctl(_cameraFd, VIDIOC_QBUF, &buf)) {
			perror("Fail to ioctl 'VIDIOC_QBUF'");
			exit(EXIT_FAILURE);
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(_cameraFd, VIDIOC_STREAMON, &type)) {
		printf("i = %d.\n", i);
		perror("Fail to ioctl 'VIDIOC_STREAMON'");
		exit(EXIT_FAILURE);
	}

	return 0;
}

int CameraV4l2::read_frame(unsigned char *dest, int &size) {
	struct v4l2_buffer buf;
	bzero(&buf, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == ioctl(_cameraFd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			return 1;
		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
		default:
			//errno_exit("VIDIOC_DQBUF");
			perror("Fail to ioctl 'VIDIOC_DQBUF'");
			exit(EXIT_FAILURE);
		}
	}
	assert(buf.index < _bufferTotal);
	if (dest != NULL) {
//		printf("copy to dest memory!\n");
		memcpy(dest, _frameBufferArray[buf.index].start, _frameBufferArray[buf.index].length);
		size = _frameBufferArray[buf.index].length;
	}

	if (-1 == ioctl(_cameraFd, VIDIOC_QBUF, &buf)) {
		perror("Fail to ioctl 'VIDIOC_QBUF'");
		exit(EXIT_FAILURE);
	}
//	printf("read ok!\n");

	return 0;
}

void CameraV4l2::stop_capturing() {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(_cameraFd, VIDIOC_STREAMOFF, &type)) {
		perror("Fail to ioctl 'VIDIOC_STREAMOFF'\n");
		exit(EXIT_FAILURE);
	}	//stream off

	unsigned int i;

	for (i = 0; i < _bufferTotal; i++) {
		if (-1
				== munmap(_frameBufferArray[i].start,
						_frameBufferArray[i].length)) {
			exit(EXIT_FAILURE);
		}
	}

	free(_frameBufferArray);

	if (-1 == close(_cameraFd)) {
		perror("Fail to close fd");
		exit(EXIT_FAILURE);
	}

	return;
}

} /* namespace BYL */

//int main() {
//	BYL::CameraV4l2 camera;
//
//	camera.setCaptureSize(320, 240);
//	camera.setCaptureFormat(V4L2_PIX_FMT_MJPEG);
//	camera.openDevice("/dev/video0");
//	camera.saveFrameData("test.jpg");
//
//	return 0;
//}
