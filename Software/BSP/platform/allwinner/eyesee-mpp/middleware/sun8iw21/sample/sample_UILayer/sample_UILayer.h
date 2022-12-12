
#ifndef _SAMPLE_VO_H_
#define _SAMPLE_VO_H_

#include <linux/fb.h> 

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUILayerCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUILayerCmdLineParam;

typedef struct SampleUILayerConfig
{
    int mPicWidth;
    int mPicHeight;
    int mDisplayWidth;
    int mDisplayHeight;
    VO_LAYER mTestUILayer;
    PIXEL_FORMAT_E mBitmapFormat; //MM_PIXEL_FORMAT_RGB_8888
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
    int mTestDuration;  //unit:s
}SampleUILayerConfig;

//a framebuffer device structure;
typedef struct fbdev{
    int nFbFd;
    unsigned long fb_mem_offset;
    void *fb_mem;
    struct fb_fix_screeninfo fb_fix;
    struct fb_var_screeninfo fb_var;
    char dev[20];   //"/ dev/fb0"
} FBDEV, *PFBDEV;

typedef struct SampleUILayerContext
{
    SampleUILayerCmdLineParam mCmdLinePara;
    SampleUILayerConfig mConfigPara;

    FBDEV mFbDev;
    int mUILayer;
    VO_VIDEO_LAYER_ATTR_S mUILayerAttr;
    
    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mTestUILayer;
    VO_VIDEO_LAYER_ATTR_S mTestUILayerAttr;
    VO_CHN mTestVOChn;
    VIDEO_FRAME_INFO_S mTestUIFrame;
}SampleUILayerContext;
int initSampleUILayerContext(SampleUILayerContext *pThiz);
int destroySampleUILayerContext(SampleUILayerContext *pThiz);

#endif  /* _SAMPLE_VO_H_ */

