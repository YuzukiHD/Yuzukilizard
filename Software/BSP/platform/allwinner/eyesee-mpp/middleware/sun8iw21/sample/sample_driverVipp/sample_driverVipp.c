
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h> 
#include <sys/ioctl.h> 
#include <sys_linux_ioctl.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <media/sunxi_camera_v2.h>

#include <cdx_list.h>
#include "sample_driverVipp.h"

#define aloge(fmt, arg...) printf("<%s:%d> " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define alogw(fmt, arg...) printf("<%s:%d> " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define alogd(fmt, arg...) printf("<%s:%d> " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define alogv(fmt, arg...)

#define MEDIA_DEVICE    "/dev/media0"
#define SUNXI_VIDEO     "vin_video"

SampleDriverVippContext* constructSampleDriverVippContext()
{
    SampleDriverVippContext *pContext = (SampleDriverVippContext*)malloc(sizeof(SampleDriverVippContext));
    memset(pContext, 0, sizeof(SampleDriverVippContext));
    pthread_condattr_t stCondAttr;
    pthread_condattr_init(&stCondAttr);
    pthread_condattr_setclock(&stCondAttr, CLOCK_MONOTONIC);
    int ret = pthread_cond_init(&pContext->mCondition, &stCondAttr);
    if (ret != 0)
    {
        aloge("pthread cond init fail(%d)", ret);
    }
    ret = pthread_mutex_init(&pContext->mLock, NULL);
    if (ret != 0)
    {
        aloge("pthread mutex init fail(%d)", ret);
    }
    INIT_LIST_HEAD(&pContext->mMediaEntityInfoList);
    return pContext;
}

int destructSampleDriverVippContext(SampleDriverVippContext *pContext)
{
    if(!list_empty(&pContext->mMediaEntityInfoList))
    {
        aloge("fatal error! mMediaEntityInfoList is not empty!");
    }
    if(pContext->mpVippVideoBufferArray != NULL)
    {
        aloge("fatal error! mpVippVideoBufferArray is not null!");
    }
    pthread_cond_destroy(&pContext->mCondition);
    pthread_mutex_destroy(&pContext->mLock);
    free(pContext);
    return 0;
}

static int convertParamToV4L2PixFmt(int param)
{
    int nV4L2PixFmt = V4L2_PIX_FMT_NV21;
    switch (param) 
    {
        case 0:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_SBGGR8;
            break;
        }
        case 1:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_YUV420M;
            break;
        }
        case 2:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_YUV420;
            break;
        }
        case 3:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_NV12M;
            break;
        }
        case 4:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_NV12;
            break;
        }
        case 5:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_SBGGR10;
            break;
        }
        case 6:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_SBGGR12;
            break;
        }
        case 7:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_LBC_2_5X;
            break;
        }
        case 8:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_NV21M;
            break;
        }
        case 9:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_NV21;
            break;
        }
        default:
        {
            nV4L2PixFmt = V4L2_PIX_FMT_YUV420M;
            break;
        }
    }
    return nV4L2PixFmt;
}
static int loadSampleDriverVippConfig(SampleDriverVippContext *pContext, int argc, char *argv[])
{
    pContext->mConfigPara.mDevId = 0;
    pContext->mConfigPara.mCaptureWidth = 1920;
    pContext->mConfigPara.mCaptureHeight = 1080;
    pContext->mConfigPara.mV4L2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mConfigPara.mV4L2MemType = V4L2_MEMORY_MMAP;
    pContext->mConfigPara.mV4L2BufNum = 5;
    pContext->mConfigPara.mPixFmt = V4L2_PIX_FMT_NV21M;
    pContext->mConfigPara.mFrameRate = 60;
    pContext->mConfigPara.mTestFrameCount = 0;
    pContext->mConfigPara.mStoreCount = 3;
    pContext->mConfigPara.mStoreInterval = 60;
    strncpy(pContext->mConfigPara.mStoreDirectory, "/mnt/extsd", sizeof(pContext->mConfigPara.mStoreDirectory));

    if (argc > 1) 
    {
        int nParamIndex = 0;
        for(nParamIndex=1; nParamIndex < argc; nParamIndex++)
        {
            switch(nParamIndex)
            {
                case 1:
                {
                    pContext->mConfigPara.mDevId = atoi(argv[1]);
                    break;
                }
                case 2:
                {
                    pContext->mConfigPara.mCaptureWidth = atoi(argv[2]);
                    break;
                }
                case 3:
                {
                    pContext->mConfigPara.mCaptureHeight = atoi(argv[3]);
                    break;
                }
                case 4:
                {
                    pContext->mConfigPara.mPixFmt = convertParamToV4L2PixFmt(atoi(argv[4]));
                    break;
                }
                case 5:
                {
                    pContext->mConfigPara.mFrameRate = atoi(argv[5]);
                    break;
                }
                case 6:
                {
                    pContext->mConfigPara.mTestFrameCount = atoi(argv[6]);
                    break;
                }
                case 7:
                {
                    pContext->mConfigPara.mStoreCount = atoi(argv[7]);
                    break;
                }
                case 8:
                {
                    pContext->mConfigPara.mStoreInterval = atoi(argv[8]);
                    break;
                }
                case 9:
                {
                    sprintf(pContext->mConfigPara.mStoreDirectory, "%s", argv[9]);
                    break;
                }
                default:
                {
                    aloge("fatal error! param index[%d] exceeds, argc[%d]", nParamIndex, argc);
                    break;
                }
            }
        }
    }
    else 
    {
        printf("please select the video device: 0-video0 1-video1 ......\n");
        scanf("%d", &pContext->mConfigPara.mDevId);

        printf("please input the capture resolution: width height......\n");
        scanf("%d %d", &pContext->mConfigPara.mCaptureWidth, &pContext->mConfigPara.mCaptureHeight);

        printf("please input the pixel format: 3:NV12M, 4:NV12, 8:NV21M, 9:NV21......\n");
        int nPixelFormat = 0;
        scanf("%d", &nPixelFormat);
        pContext->mConfigPara.mPixFmt = convertParamToV4L2PixFmt(nPixelFormat);

        printf("please input the fps: ......\n");
        scanf("%d", &pContext->mConfigPara.mFrameRate);

        printf("please input the test frame count: ......\n");
        scanf("%d", &pContext->mConfigPara.mTestFrameCount);

        printf("please input store count: ......\n");
        scanf("%d", &pContext->mConfigPara.mStoreCount);

        printf("please input store interval: ......\n");
        scanf("%d", &pContext->mConfigPara.mStoreInterval);
        
        printf("please input the frame saving path......\n");
        scanf("%64s", pContext->mConfigPara.mStoreDirectory);
    }
    return 0;
}

SampleDriverVippContext *gpSampleDriverVippContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleDriverVippContext)
    {
        pthread_mutex_lock(&gpSampleDriverVippContext->mLock);
        gpSampleDriverVippContext->mExitFlag++;
        alogd("signal ExitFlag:%d", gpSampleDriverVippContext->mExitFlag);
        pthread_cond_signal(&gpSampleDriverVippContext->mCondition);
        pthread_mutex_unlock(&gpSampleDriverVippContext->mLock);
    }
}

int savePicture(SampleDriverVippContext *pContext, VippVideoBuffer *pBuffer)
{
    //make file name, e.g., /mnt/extsd/pic[0].NV21M
    int i;
    char strPixFmt[16] = {0};
    switch(pContext->mConfigPara.mPixFmt)
    {
        case V4L2_PIX_FMT_NV12M:
        {
            strcpy(strPixFmt,"nv12m");
            break;
        }
        case V4L2_PIX_FMT_NV12:
        {
            strcpy(strPixFmt,"nv12");
            break;
        }
        case V4L2_PIX_FMT_NV21M:
        {
            strcpy(strPixFmt,"nv21m");
            break;
        }
        case V4L2_PIX_FMT_NV21:
        {
            strcpy(strPixFmt,"nv21");
            break;
        }
        default:
        {
            strcpy(strPixFmt,"unknown");
            break;
        }
    }
    char strFilePath[MAX_FILE_PATH_SIZE];
    snprintf(strFilePath, MAX_FILE_PATH_SIZE, "%s/pic[%d].%s", pContext->mConfigPara.mStoreDirectory, pContext->mStoreCounter, strPixFmt);

    FILE *fpPic = fopen(strFilePath, "wb");
    if(fpPic != NULL)
    {
        for(i = 0; i < pBuffer->nplanes; i++)
        {
            if(pBuffer->planes[i].bytesused > 0)
            {
                fwrite(pBuffer->planes[i].mem, 1, pBuffer->planes[i].bytesused, fpPic);
                alogd("fwrite index[%d], plane[%d]=[%p], bytesused=[%d], size=[%d]", 
                    pBuffer->index, i, pBuffer->planes[i].mem, pBuffer->planes[i].bytesused, pBuffer->planes[i].size);
            }
        }
        fclose(fpPic);
        alogd("store pic in file[%s]", strFilePath);
    } 
    return 0;
}

#define ISP_RUN 0

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    printf("sample_driverVipp running!\n");
    SampleDriverVippContext *pContext = constructSampleDriverVippContext();
    gpSampleDriverVippContext = pContext;
    loadSampleDriverVippConfig(pContext, argc, argv);
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }
    alogd("before open MEDIA_DEVICE[%s]", MEDIA_DEVICE);
    pContext->mMediaDevFd = open(MEDIA_DEVICE, O_RDWR);
    alogd("after open MEDIA_DEVICE[%s]", MEDIA_DEVICE);
    if(pContext->mMediaDevFd < 0)
    {
        aloge("fatal error! open [%s] fail!", MEDIA_DEVICE);
    }
    //query media device info, media entity infos.
    int ret = 0;
    ret = ioctl(pContext->mMediaDevFd, MEDIA_IOC_DEVICE_INFO, &pContext->mMediaDeviceInfo);
    if (ret < 0) 
    {
        aloge("Unable to retrieve media device information for "
               "device %s (%s)\n", MEDIA_DEVICE, strerror(errno));
        goto _err0;
    }
    alogd("mediaDeviceInfo:\n"
        "driver:[%s]\n"
        "model:[%s]\n"
        "serial:[%s]\n"
        "bus_info:[%s]\n"
        "media_version:%d, hw_revision:%d, driver_version:%d",
        pContext->mMediaDeviceInfo.driver, pContext->mMediaDeviceInfo.model, pContext->mMediaDeviceInfo.serial, pContext->mMediaDeviceInfo.bus_info, 
        pContext->mMediaDeviceInfo.media_version, pContext->mMediaDeviceInfo.hw_revision, pContext->mMediaDeviceInfo.driver_version);
    MediaEntityInfo *pEntityInfo = NULL;
    unsigned int id;
    for (id = 0; ; id = pEntityInfo->mKernelDesc.id)
    {
        pEntityInfo = malloc(sizeof(MediaEntityInfo));
        memset(pEntityInfo, 0, sizeof(MediaEntityInfo));
        pEntityInfo->mFd = -1;
        pEntityInfo->mKernelDesc.id = id | MEDIA_ENT_ID_FLAG_NEXT;
        ret = ioctl(pContext->mMediaDevFd, MEDIA_IOC_ENUM_ENTITIES, &pEntityInfo->mKernelDesc);
        if (ret < 0) 
        {
            free(pEntityInfo);
            if (errno == EINVAL)
            {
                int cnt = 0;
                struct list_head *pList;
                list_for_each(pList, &pContext->mMediaEntityInfoList)
                {
                    cnt++;
                }            
                alogd("enum media entity done! num:%d", cnt);
                break;
            }
            else
            {
                aloge("fatal error! enum entity error(%d)", ret);
                break;
            }
        }
        list_add_tail(&pEntityInfo->mList, &pContext->mMediaEntityInfoList);

        alogd("media_entity_desc: id[0x%x], type[0x%x], name[%s], pads[%d], links[%d], major/minor[%d/%d]", 
            pEntityInfo->mKernelDesc.id, pEntityInfo->mKernelDesc.type, pEntityInfo->mKernelDesc.name, 
            pEntityInfo->mKernelDesc.pads, pEntityInfo->mKernelDesc.links,
            pEntityInfo->mKernelDesc.v4l.major, pEntityInfo->mKernelDesc.v4l.minor);
        /* Find the corresponding device name. */
        int nMediaEntityType = pEntityInfo->mKernelDesc.type & MEDIA_ENT_TYPE_MASK;
        if(nMediaEntityType != MEDIA_ENT_T_DEVNODE && nMediaEntityType != MEDIA_ENT_T_V4L2_SUBDEV)
        {
            alogw("media entity type is not devnode or v4l2_subdev");
            continue;
        }

        char sysname[32];
        char target[1024];
        sprintf(sysname, "/sys/dev/char/%u:%u", pEntityInfo->mKernelDesc.v4l.major, pEntityInfo->mKernelDesc.v4l.minor);
        ret = readlink(sysname, target, sizeof(target));
        if (ret < 0)
        {
            aloge("fatal error! read link [%s] fail(%d)", sysname, ret);
            continue;
        }
        target[ret] = '\0';
        alogd("sysname[%s], target[%s]", sysname, target);
        char *p;
        p = strrchr(target, '/');
        if (p == NULL)
        {
            alogw("target[%s] does not contain '/'", target);
            continue;
        }
        
        char devname[32];
        sprintf(devname, "/dev/%s", p + 1);
        struct stat devstat;
        ret = stat(devname, &devstat);
        if (ret < 0)
        {
            aloge("fatal error! stat dev[%s] fail(%d)!", devname, ret);
            continue;
        }
        strcpy(pEntityInfo->mDevName, devname);
        alogd("make devName[%s]", pEntityInfo->mDevName);
    }

    //find media entity which matches vipp by vippId, open device of this media entity.
    char media_entity_name[32];
    snprintf(media_entity_name, sizeof(media_entity_name), SUNXI_VIDEO"%d", (int)pContext->mConfigPara.mDevId);
    alogd("name of media_entity_desc we wanted is %s\n", media_entity_name);
    MediaEntityInfo *pEntry, *pTemp;
    list_for_each_entry(pEntry, &pContext->mMediaEntityInfoList, mList)
    {
        if (strcmp(pEntry->mKernelDesc.name, media_entity_name) == 0)
        {
            pContext->mpVippMediaEntityInfo = pEntry;
            break;
        }
    }
    if (pContext->mpVippMediaEntityInfo == NULL) 
    {
        aloge("fatal error! can not get entity by name %s\n", media_entity_name);
    }

    if (pContext->mpVippMediaEntityInfo->mFd != -1)
    {
        aloge("fatal error! why media entity fd[%d] is not -1?", pContext->mpVippMediaEntityInfo->mFd);
    }

    pContext->mpVippMediaEntityInfo->mFd = open(pContext->mpVippMediaEntityInfo->mDevName, O_RDWR | O_NONBLOCK, 0);
    if (pContext->mpVippMediaEntityInfo->mFd < 0)
    {
        aloge("fatal error! Failed to open subdev device node %s\n", pContext->mpVippMediaEntityInfo->mDevName);
    }
    #if 0
    // set top clock
    struct vin_top_clk clk;
    clk.clk_rate = rate;
    if (-1 == ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_SET_TOP_CLK, &clk)) 
    {
        aloge("fatal error! video%d VIDIOC_SET_TOP_CLK failed\n", (int)video->id);
        return -1;
    }
    #endif
    memset(&pContext->mV4L2Input, 0, sizeof(struct v4l2_input));
    pContext->mV4L2Input.index = 0;
    if (-1 == ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_S_INPUT, &pContext->mV4L2Input)) 
    {
        aloge("fatal error! VIDIOC_S_INPUT %d error!", pContext->mV4L2Input.index);
    }

    memset(&pContext->mV4L2StreamParam, 0, sizeof(struct v4l2_streamparm));
    pContext->mV4L2StreamParam.type = pContext->mConfigPara.mV4L2BufType;
    pContext->mV4L2StreamParam.parm.capture.timeperframe.numerator = 1;
    pContext->mV4L2StreamParam.parm.capture.timeperframe.denominator = pContext->mConfigPara.mFrameRate; // 30;
    pContext->mV4L2StreamParam.parm.capture.capturemode = V4L2_MODE_VIDEO;
    pContext->mV4L2StreamParam.parm.capture.reserved[0] = 0;/*when different video have the same sensor source, 1:use sensor current win, 0:find the nearest win*/
    pContext->mV4L2StreamParam.parm.capture.reserved[1] = 0; /*2:comanding, 1: wdr, 0: normal*/
    if (-1 == ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_S_PARM, &pContext->mV4L2StreamParam)) 
    {
        aloge("VIDIOC_S_PARM error");
    }

    memset(&pContext->mV4L2Fmt, 0, sizeof(struct v4l2_format));
    pContext->mV4L2Fmt.type = pContext->mConfigPara.mV4L2BufType;
    pContext->mV4L2Fmt.fmt.pix_mp.pixelformat = pContext->mConfigPara.mPixFmt;
    pContext->mV4L2Fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    pContext->mV4L2Fmt.fmt.pix_mp.width = pContext->mConfigPara.mCaptureWidth;
    pContext->mV4L2Fmt.fmt.pix_mp.height = pContext->mConfigPara.mCaptureHeight;
    pContext->mV4L2Fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
    if (-1 == ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_S_FMT, &pContext->mV4L2Fmt)) 
    {
        aloge("VIDIOC_S_FMT error!");
    }

    if (-1 == ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_G_FMT, &pContext->mV4L2Fmt)) 
    {
        aloge("VIDIOC_G_FMT error!");
    } 
    else 
    {
        alogd("VIDIOC_G_FMT: resolution: %dx%d, num_planes: %d", 
            pContext->mV4L2Fmt.fmt.pix_mp.width, pContext->mV4L2Fmt.fmt.pix_mp.height, 
            pContext->mV4L2Fmt.fmt.pix_mp.num_planes);
    }

    if (-1 == ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_G_PARM, &pContext->mV4L2StreamParam))
    {
        aloge("VIDIOC_G_PARM error");
    }
    else
    {
        alogd("VIDIOC_G_PARM: capturemode:0x%x", pContext->mV4L2StreamParam.parm.capture.capturemode);
    }

    memset(&pContext->mV4L2Rb, 0, sizeof(struct v4l2_requestbuffers));
    pContext->mV4L2Rb.count = pContext->mConfigPara.mV4L2BufNum;
    pContext->mV4L2Rb.type = pContext->mConfigPara.mV4L2BufType;
    pContext->mV4L2Rb.memory = pContext->mConfigPara.mV4L2MemType;
    alogd("devname[%s] reqbufs: nr[%d], type[%d]", pContext->mpVippMediaEntityInfo->mDevName, pContext->mV4L2Rb.count, pContext->mV4L2Rb.type);
    usleep(100*1000);
    ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_REQBUFS, &pContext->mV4L2Rb);
    usleep(100*1000);
    alogd("devname[%s] reqbufs: nr[%d], type[%d] done, ret[%d]", pContext->mpVippMediaEntityInfo->mDevName, pContext->mV4L2Rb.count, pContext->mV4L2Rb.type, ret);
    if (ret < 0) 
    {
        aloge("fatal error! devname[%s]: unable to request buffers (%d)", pContext->mpVippMediaEntityInfo->mDevName, errno);
    }

    if (pContext->mV4L2Rb.count > pContext->mConfigPara.mV4L2BufNum)
    {
        aloge("fatal error! devname[%s]: driver needs more buffers (%u) than available (%d).",
               pContext->mpVippMediaEntityInfo->mDevName, pContext->mV4L2Rb.count, pContext->mConfigPara.mV4L2BufNum);
    }
    pContext->mVippVideoBufferCount = pContext->mV4L2Rb.count;
    pContext->mpVippVideoBufferArray = calloc(pContext->mV4L2Rb.count, sizeof(VippVideoBuffer));
    if(NULL == pContext->mpVippVideoBufferArray)
    {
        aloge("fatal error! calloc fail.");
    }
    int i, j;
    for(i = 0; i < pContext->mV4L2Rb.count; i++)
    {
        pContext->mpVippVideoBufferArray[i].index = i;
        pContext->mpVippVideoBufferArray[i].nplanes = pContext->mV4L2Fmt.fmt.pix_mp.num_planes;
        pContext->mpVippVideoBufferArray[i].planes = calloc(pContext->mV4L2Fmt.fmt.pix_mp.num_planes, sizeof(VippVideoBuffer));
        if (NULL == pContext->mpVippVideoBufferArray[i].planes) 
        {
            aloge("fatal error! planes struct calloc failed!\n");
        }
    }
    
    /* Map the buffers. */
    for (i = 0; i < pContext->mV4L2Rb.count; ++i) 
    {
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        memset(planes, 0, sizeof planes);
        memset(&pContext->mTmpV4L2Vb, 0, sizeof(struct v4l2_buffer));

        pContext->mTmpV4L2Vb.type = pContext->mConfigPara.mV4L2BufType;
        pContext->mTmpV4L2Vb.memory = pContext->mConfigPara.mV4L2MemType;
        pContext->mTmpV4L2Vb.index = i;
        pContext->mTmpV4L2Vb.length = pContext->mV4L2Fmt.fmt.pix_mp.num_planes;
        pContext->mTmpV4L2Vb.m.planes = planes;
        ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_QUERYBUF, &pContext->mTmpV4L2Vb);
        if (ret < 0) 
        {
            aloge("fatal error! devname[%s]: unable to query buffer %d (%d).\n", pContext->mpVippMediaEntityInfo->mDevName, i, errno);
        }
        switch (pContext->mConfigPara.mV4L2MemType) 
        {
            case V4L2_MEMORY_MMAP:
            {
                for (j = 0; j < pContext->mV4L2Fmt.fmt.pix_mp.num_planes; j++) 
                {
                    pContext->mpVippVideoBufferArray[i].planes[j].size = pContext->mTmpV4L2Vb.m.planes[j].length;
                    pContext->mpVippVideoBufferArray[i].planes[j].mem 
                        = mmap(NULL /* start anywhere */ ,
                             pContext->mTmpV4L2Vb.m.planes[j].length,
                             PROT_READ | PROT_WRITE /* required */ ,
                             MAP_SHARED /* recommended */ ,
                             pContext->mpVippMediaEntityInfo->mFd, 
                             pContext->mTmpV4L2Vb.m.planes[j].m.mem_offset);
                    if (MAP_FAILED == pContext->mpVippVideoBufferArray[i].planes[j].mem) 
                    {
                        aloge("fatal error! devname[%s]: unable to map buffer %d plane %d, errno(%d)",
                               pContext->mpVippMediaEntityInfo->mDevName, i, j, errno);
                    }
                    alogd("devname[%s]: buffer %d planes %d mapped at address %p\n",
                        pContext->mpVippMediaEntityInfo->mDevName, i, (int)j,
                        pContext->mpVippVideoBufferArray[i].planes[j].mem);
                }
                break;
            }
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
            aloge("fatal error! unknown v4l2 mem type[0x%x]", pContext->mConfigPara.mV4L2MemType);
            break;
        }
    }
    for(i = 0; i < pContext->mV4L2Rb.count; i++)
    {
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        memset(&pContext->mTmpV4L2Vb, 0, sizeof(struct v4l2_buffer));
        memset(planes, 0, sizeof planes);
        pContext->mTmpV4L2Vb.index = pContext->mpVippVideoBufferArray[i].index;
        pContext->mTmpV4L2Vb.type = pContext->mConfigPara.mV4L2BufType;
        pContext->mTmpV4L2Vb.memory = pContext->mConfigPara.mV4L2MemType;
        pContext->mTmpV4L2Vb.length = pContext->mV4L2Fmt.fmt.pix_mp.num_planes;
        pContext->mTmpV4L2Vb.m.planes = planes;
        switch(pContext->mConfigPara.mV4L2MemType)
        {
            case V4L2_MEMORY_MMAP:
            {
                break;
            }
            default:
            {
                aloge("fatal error! unsupport v4l2 mem type:0x%x", pContext->mConfigPara.mV4L2MemType);
                break;
            }
        }
        ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_QBUF, &pContext->mTmpV4L2Vb);
        if (ret < 0) 
        {
            aloge("fatal error! devname[%s]: unable to queue buffer index %u/%u errno(%d)",
                   pContext->mpVippMediaEntityInfo->mDevName, pContext->mTmpV4L2Vb.index, pContext->mV4L2Rb.count, errno);
        }
    }
    int nV4L2BufType = pContext->mConfigPara.mV4L2BufType;
    ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_STREAMON, &nV4L2BufType);
    if (ret < 0)
    {
        aloge("fatal error! stream on fail, ret(%d), errno(%d)", ret, errno);
    }

#if ISP_RUN //isp operation will implement in future.
    /* open isp */
    AW_MPI_ISP_Run(pContext->mIspDev);
#endif
    usleep(100*1000);   //let vipp run some time.

    //begin to try to get frame and release frame!
    while(1)
    {
        int timeout = 2*1000;   //unit:ms
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(pContext->mpVippMediaEntityInfo->mFd, &fds);

        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        // poll if frame is ready
        r = select(pContext->mpVippMediaEntityInfo->mFd + 1, &fds, NULL, NULL, &tv);
        pthread_mutex_lock(&pContext->mLock);
        if(pContext->mExitFlag > 0)
        {
            alogd("check ExitFlag:%d, exit loop of getting frames", pContext->mExitFlag);
            pthread_mutex_unlock(&pContext->mLock);
            break;
        }
        pthread_mutex_unlock(&pContext->mLock);
        if (r < 0)
        {
            aloge("fatal error! devname[%s] select error!", pContext->mpVippMediaEntityInfo->mDevName);
            continue;
        }
        else if (0 == r) 
        {
            alogw("Be careful! devname[%s] select timeout!", pContext->mpVippMediaEntityInfo->mDevName);
            continue;
        }

        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        int i = 0;

        memset(&pContext->mTmpV4L2Vb, 0, sizeof pContext->mTmpV4L2Vb);
        memset(planes, 0, sizeof planes);

        pContext->mTmpV4L2Vb.type = pContext->mConfigPara.mV4L2BufType;
        pContext->mTmpV4L2Vb.memory = pContext->mConfigPara.mV4L2MemType;
        pContext->mTmpV4L2Vb.length = pContext->mV4L2Fmt.fmt.pix_mp.num_planes;
        pContext->mTmpV4L2Vb.m.planes = planes;
        // get frame
        ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_DQBUF, &pContext->mTmpV4L2Vb);
        if (ret < 0) 
        {
            aloge("fatal error! devname[%s]: unable to dequeue buffer index %u/%u (%d)",
                pContext->mpVippMediaEntityInfo->mDevName, pContext->mTmpV4L2Vb.index, pContext->mV4L2Rb.count, errno);
            continue;
        }

        assert(pContext->mTmpV4L2Vb.index < pContext->mV4L2Rb.count);

        VippVideoBuffer *pBuffer = &pContext->mpVippVideoBufferArray[pContext->mTmpV4L2Vb.index];
        pBuffer->bytesused = pContext->mTmpV4L2Vb.bytesused;
        pBuffer->frame_cnt = pContext->mTmpV4L2Vb.reserved;
        pBuffer->exp_time = pContext->mTmpV4L2Vb.reserved2;
        pBuffer->timestamp = pContext->mTmpV4L2Vb.timestamp;
        pBuffer->error = !!(pContext->mTmpV4L2Vb.flags & V4L2_BUF_FLAG_ERROR);
        for (i = 0; i < pContext->mV4L2Fmt.fmt.pix_mp.num_planes; i++) 
        {
            pBuffer->planes[i].bytesused = pContext->mTmpV4L2Vb.m.planes[i].bytesused;
            pBuffer->planes[i].mem_phy = pContext->mTmpV4L2Vb.m.planes[i].m.mem_offset;
        }

        //process frame, e.g., save it to directory.
        if(0 == pContext->mFrameCounter%pContext->mConfigPara.mStoreInterval
            && pContext->mStoreCounter < pContext->mConfigPara.mStoreCount)
        {
            //save picture
            savePicture(pContext, pBuffer);
            pContext->mStoreCounter++;
        }
        pContext->mFrameCounter++;

        //release frame.
        memset(&pContext->mTmpV4L2Vb, 0, sizeof(struct v4l2_buffer));
        memset(planes, 0, sizeof planes);
        pContext->mTmpV4L2Vb.index = pBuffer->index;
        pContext->mTmpV4L2Vb.type = pContext->mConfigPara.mV4L2BufType;
        pContext->mTmpV4L2Vb.memory = pContext->mConfigPara.mV4L2MemType;
        pContext->mTmpV4L2Vb.length = pContext->mV4L2Fmt.fmt.pix_mp.num_planes;
        pContext->mTmpV4L2Vb.m.planes = planes;
        switch(pContext->mConfigPara.mV4L2MemType)
        {
            case V4L2_MEMORY_MMAP:
            {
                break;
            }
            default:
            {
                aloge("fatal error! unsupport v4l2 mem type:0x%x", pContext->mConfigPara.mV4L2MemType);
                break;
            }
        }
        ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_QBUF, &pContext->mTmpV4L2Vb);
        if (ret < 0) 
        {
            aloge("fatal error! devname[%s]: unable to queue buffer index %u/%u errno(%d)",
                   pContext->mpVippMediaEntityInfo->mDevName, pContext->mTmpV4L2Vb.index, pContext->mV4L2Rb.count, errno);
        }
        //judge whether meet the condition of exit.
        if(pContext->mFrameCounter > pContext->mConfigPara.mTestFrameCount && pContext->mConfigPara.mTestFrameCount > 0)
        {
            alogd("meet exiting condition[%d]>[%d], exit now!",pContext->mFrameCounter, pContext->mConfigPara.mTestFrameCount);
            break;
        }
    }

    //stream off
    ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_STREAMOFF, &nV4L2BufType);
    if (ret < 0)
    {
        aloge("fatal error! stream off fail, ret(%d), errno(%d)", ret, errno);
    }
    
    //free v4l2 buffers.
    switch (pContext->mConfigPara.mV4L2MemType) 
    {
        case V4L2_MEMORY_MMAP:
        {
            for (i = 0; i < pContext->mV4L2Rb.count; i++)
            {
                for (j = 0; j < pContext->mpVippVideoBufferArray[i].nplanes; j++) 
                {
                    VippVideoPlane *plane = &pContext->mpVippVideoBufferArray[i].planes[j];
                    if (plane->mem == NULL)
                    {
                        continue;
                    }
                    if (munmap(plane->mem, plane->size)) 
                    {
                        aloge("fatal error! devname[%s]: unable to unmap buffer %d (%d)",
                            pContext->mpVippMediaEntityInfo->mDevName, i, errno);
                    }
                    plane->mem = NULL;
                    plane->size = 0;
                }
            }
            break;
        }
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

    memset(&pContext->mV4L2Rb, 0, sizeof pContext->mV4L2Rb);
    pContext->mV4L2Rb.count = 0;
    pContext->mV4L2Rb.type = pContext->mConfigPara.mV4L2BufType;
    pContext->mV4L2Rb.memory = pContext->mConfigPara.mV4L2MemType;

    ret = ioctl(pContext->mpVippMediaEntityInfo->mFd, VIDIOC_REQBUFS, &pContext->mV4L2Rb);
    if (ret < 0) 
    {
        aloge("fatal error! devname[%s]: unable to release buffers (%d)", pContext->mpVippMediaEntityInfo->mDevName, errno);
    }
    
#if ISP_RUN
    eRet = AW_MPI_ISP_Stop(pContext->mIspDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
#endif

    for(i = 0; i < pContext->mVippVideoBufferCount; i++)
    {
        if(pContext->mpVippVideoBufferArray[i].planes)
        {
            free(pContext->mpVippVideoBufferArray[i].planes);
            pContext->mpVippVideoBufferArray[i].planes = NULL;
        }
    }
    free(pContext->mpVippVideoBufferArray);
    pContext->mpVippVideoBufferArray = NULL;

    //close vipp device fd.
    if(pContext->mpVippMediaEntityInfo)
    {
        if(pContext->mpVippMediaEntityInfo->mFd > 0)
        {
            close(pContext->mpVippMediaEntityInfo->mFd);
            pContext->mpVippMediaEntityInfo->mFd = -1;
        }
    }

    //free media entity info list.
    list_for_each_entry_safe(pEntry, pTemp, &pContext->mMediaEntityInfoList, mList)
    {
        list_del(&pEntry->mList);
        free(pEntry);
    }

_err0:
    //close media device fd.
    if(pContext->mMediaDevFd > 0)
    {
        close(pContext->mMediaDevFd);
        pContext->mMediaDevFd = -1;
    }

    destructSampleDriverVippContext(pContext);
    gpSampleDriverVippContext = pContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

