/*
 * CameraV4l2.h
 *
 *  Created on: 2016年6月30日
 *      Author: A
 */

#ifndef LIVE_IPCPROGRAM_CAMERAV4L2_H_
#define LIVE_IPCPROGRAM_CAMERAV4L2_H_

#include <string>

using namespace std;

namespace BYL {

class CameraV4l2 {
public:
	CameraV4l2();
	virtual ~CameraV4l2();
	int openDevice(string const &name);
	void setCaptureSize(int width, int height);
	void setCaptureFormat(unsigned int type);
	int getCaptureData(unsigned char *&data, int &size);
	int freeCaptureData(unsigned char *&data);
	int read_frame(unsigned char *dest, int &size);
	int saveFrameData(string const &fileName);
	int closeDevice();

private:
	unsigned int _captureWidth;
	unsigned int _captureHeight;
	int _cameraFd;
	unsigned int _captureFormat;
	struct FrameBuffer; //forward
	FrameBuffer *_frameBufferArray;
	unsigned int _bufferTotal;


	struct FrameBuffer {
		unsigned char *start;
		int length;
	};

	int configureCamera();
	int init_camera_device();
	int init_mmap();
	int start_capturing();
	void stop_capturing();
};

} /* namespace BYL */

#endif /* LIVE_IPCPROGRAM_CAMERAV4L2_H_ */
