#ifndef _SAMPLE_DRIVERVIPP_H_
#define _SAMPLE_DRIVERVIPP_H_

#include <stdbool.h>
#include <pthread.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include <cdx_list_type.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleDriverVippConfig
{
    int mDevId;
    int mCaptureWidth;
    int mCaptureHeight;
    enum v4l2_buf_type mV4L2BufType;	//V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
    enum v4l2_memory mV4L2MemType;  //V4L2_MEMORY_MMAP
    int mV4L2BufNum;
    int mPixFmt;	//V4L2_PIX_FMT_NV21
    int mFrameRate;
    int mTestFrameCount;  //0 mean infinite

	int mStoreCount;	//the picture number of storing. 0 means don't store pictures.
	int mStoreInterval;	//n: store one picture of n pictures.
	char mStoreDirectory[MAX_FILE_PATH_SIZE];   //e.g.: /mnt/extsd
}SampleDriverVippConfig;

typedef struct MediaEntityInfo
{
    struct media_entity_desc mKernelDesc;
    char mDevName[32];
    int mFd;
    struct list_head mList;
}MediaEntityInfo;

typedef struct VippVideoPlane {
    unsigned int bytesused;
	unsigned int size;
	int dma_fd;
	void *mem;
	unsigned int  mem_phy;
}VippVideoPlane;

typedef struct VippVideoBuffer {
	unsigned int index;
	unsigned int bytesused;
	unsigned int frame_cnt;
	unsigned int exp_time;
	struct timeval timestamp;
	bool error;
	bool allocated;
	unsigned int nplanes;
	VippVideoPlane *planes;
}VippVideoBuffer;

typedef struct SampleDriverVippContext
{
    SampleDriverVippConfig mConfigPara;

    int mMediaDevFd;
    struct media_device_info mMediaDeviceInfo;
    struct list_head mMediaEntityInfoList;  //MediaEntityInfo

    MediaEntityInfo *mpVippMediaEntityInfo;
    struct v4l2_format mV4L2Fmt;
	struct v4l2_input mV4L2Input;
	struct v4l2_streamparm mV4L2StreamParam;
    struct v4l2_requestbuffers mV4L2Rb;
	struct v4l2_buffer mTmpV4L2Vb;

    int mVippVideoBufferCount;
    VippVideoBuffer *mpVippVideoBufferArray;  //VippVideoBuffer, array size is decided by mConfigPara.mV4L2BufNum
    
    int mStoreCounter;
    int mFrameCounter;
    pthread_cond_t  mCondition;
    pthread_mutex_t mLock;
    int mExitFlag;
}SampleDriverVippContext;

SampleDriverVippContext* constructSampleDriverVippContext();
int destructSampleDriverVippContext(SampleDriverVippContext *pContext);

#endif  /* _SAMPLE_DRIVERVIPP_H_ */

