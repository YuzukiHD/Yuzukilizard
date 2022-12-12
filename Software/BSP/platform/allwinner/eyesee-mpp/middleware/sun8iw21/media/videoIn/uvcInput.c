/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : uvcInput.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/12/06
  Last Modified :
  Description   : mpi_uvc functions implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "videoInput"
#include <utils/plat_log.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <cdx_list.h>

#include <UvcVirVi_Component.h>
#include <mpi_videoformat_conversion.h>
#include <ion_memmanager.h>

#define SHOW_INFO_OF_UVC

static void *uvcInput_CapThread(void *pThreadData);

static void yuv422pa_yuv422sp(unsigned char *yuyv, 
                                       unsigned char * nv21_y, unsigned char *nv21_uv, 
                                        unsigned int width, unsigned int height)
{
    int nSum =  width * height * 2;
    int i;
    for(i = 0; i < nSum; i++)
    {
        if(0 == i % 2)
        {
            *nv21_y = yuyv[i];
            nv21_y++;
        }
        else
        {
            *nv21_uv = yuyv[i];
            nv21_uv++;
        }
    }    
}
                                        
static UvcDevManager *gpUvcDevManager = NULL;

static ERRORTYPE uvc_query(int fd, int cmd_id, const char *cmd_name, struct v4l2_queryctrl *queryctrl)
{
    ERRORTYPE bResult = FAILURE;

    if(queryctrl)
    {
        queryctrl->id = cmd_id;
        if(ioctl(fd, VIDIOC_QUERYCTRL, queryctrl) < 0)
        {
            aloge("failed query %s, %s", cmd_name, strerror(errno));
        }
        else
        {
            bResult = SUCCESS;
        }
    }
    return bResult;
}


static ERRORTYPE uvcInput_SearchExitDev(UVC_DEV UvcDev, UvcChnManager ***pppUvcChnManager)
{
    ERRORTYPE bResult = FAILURE;
    
    int i = 0;
    for(; i < UVC_MAX_DEV_NUM; i++)
    {
        if(gpUvcDevManager->mUvcChnManager[i] && 0 == strcmp(gpUvcDevManager->mUvcChnManager[i]->mUvcDevName, UvcDev))
        {
            bResult = SUCCESS;
            if(pppUvcChnManager)
            {
                *pppUvcChnManager = &gpUvcDevManager->mUvcChnManager[i];
            }
            break;
        }
    }        
    return bResult;
}
ERRORTYPE uvcInput_SearchExitDevVirChn(UVC_DEV UvcDev, UVC_CHN UvcChn, UVC_CHN_MAP_S **ppChn)
{
    ERRORTYPE bResult = FAILURE;
    
    UvcChnManager **ppUvcChnManager = NULL;
    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;            
    }

    UvcChnManager *pUvcChnManager = *ppUvcChnManager;
    pthread_mutex_lock(&pUvcChnManager->mLock);

    UVC_CHN_MAP_S *pEnty = NULL;
    list_for_each_entry(pEnty, &pUvcChnManager->mChnList, mList)
    {
        if(pEnty->mUvcChn == UvcChn)
        {
            bResult = SUCCESS;
            if(ppChn)
            {
                *ppChn = pEnty;
            }
            break;            
        }
    }
    
    pthread_mutex_unlock(&pUvcChnManager->mLock);
    
    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);

    return bResult;    
}

MM_COMPONENTTYPE *uvcInput_GetChnComp(UVC_DEV UvcDev, UVC_CHN UvcChn)
{
    UVC_CHN_MAP_S *pChn;   
    if(uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, &pChn) != SUCCESS)
    {
        return NULL;
    }
    return pChn->mUvcComp;
}


ERRORTYPE uvcInput_Construct(UVC_DEV UvcDev, int fd)
{
    ERRORTYPE bResult = FAILURE;    
    
    UvcChnManager **ppUvcChnManager = NULL;    
    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
    if(SUCCESS == uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager))
    {
        alogd("the UVC %s had open!", UvcDev);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        bResult = SUCCESS;
        return bResult;
    }
    
    int i = 0;
    for(; i < UVC_MAX_CHN_NUM; i++)
    {
        if(!gpUvcDevManager->mUvcChnManager[i])
        {
            break;
        }
    }
    if(UVC_MAX_CHN_NUM == i)
    {
        aloge("the UVC Dev had enough, it had %d now", UVC_MAX_CHN_NUM);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;            
    }

    UvcChnManager *pUvcChnManager = (UvcChnManager *)malloc(sizeof *pUvcChnManager);
    if(NULL == pUvcChnManager)
    {
        aloge("alloc %s UVC UvcChnManager error(%s)!", UvcDev, strerror(errno));        
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;
    }

    memset(pUvcChnManager, 0, sizeof *pUvcChnManager);
    if(strlen(UvcDev) >= UVC_MAX_DEV_NAME)
    {
        aloge("the dev name %s had over %d", UvcDev, UVC_MAX_DEV_NAME);
        goto error;
    }
    memcpy(pUvcChnManager->mUvcDevName, UvcDev, strlen(UvcDev)+1);
    pUvcChnManager->mFd = fd;

    pthread_mutex_init(&pUvcChnManager->mLock, NULL);
    pthread_mutex_init(&pUvcChnManager->mRefsLock, NULL);    
    INIT_LIST_HEAD(&pUvcChnManager->mChnList);
    gpUvcDevManager->mUvcChnManager[i] = pUvcChnManager;

    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);

    bResult = SUCCESS;

    return bResult;
    
error:
    free(pUvcChnManager);    
    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    return bResult;
}

 ERRORTYPE uvcInput_Destruct(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;
    int i = 0;
    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);

    UvcChnManager **ppUvcChnManager = NULL;
    if(SUCCESS == uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager))
    {
        UvcChnManager *pUvcChnManager = NULL ;
        pUvcChnManager = *ppUvcChnManager;
       
        if(!list_empty(&pUvcChnManager->mChnList))
        {
            aloge("fatal error! the % UVC had some channel still running when destroy it!");
            goto Label;                  
        }
        //ummap
        int bufferlength = pUvcChnManager->mBufferLength;
        if(bufferlength != 0)
        {
            for(i = 0 ; i < pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt; i++)
            {
                if(pUvcChnManager->mpVideoBuffer[i])
                {
                    if(munmap(pUvcChnManager->mpVideoBuffer[i], bufferlength) < 0)
                    {
                        aloge("the %s UVC munmap buffer[%d] fail, %s", UvcDev, i, strerror(errno));                        
                    }
                }
            }
        }
 
        close(pUvcChnManager->mFd);//close UVC fd;        
        pthread_mutex_destroy(&pUvcChnManager->mLock);
        pthread_mutex_destroy(&pUvcChnManager->mRefsLock);

        for(i = 0; i < pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt; i++)
        {
            if(pUvcChnManager->mUvcCaptureFrame[i].VFrame.mpVirAddr[0])
            {
                ion_freeMem(pUvcChnManager->mUvcCaptureFrame[i].VFrame.mpVirAddr[0]);
            }
        }
        
        free(pUvcChnManager);
        *ppUvcChnManager = NULL;       

    }
    bResult = SUCCESS;
Label:
    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    return bResult;   
}

ERRORTYPE UVC_Construct(void)
{
    ERRORTYPE bResult = FAILURE;
    if(gpUvcDevManager != NULL)
    {
        return SUCCESS;
    }

    gpUvcDevManager = (UvcDevManager *)malloc(sizeof(UvcDevManager));
    if(NULL == gpUvcDevManager)
    {
        aloge("alloc gpUvcDevManager fail, %", strerror(errno));
        return bResult;
    }
    memset(gpUvcDevManager, 0, sizeof *gpUvcDevManager); 
    pthread_mutex_init(&gpUvcDevManager->mUvcDevManagerLock, NULL);
    

    return SUCCESS;    
}

ERRORTYPE UVC_Destruct(void)
{
    if(gpUvcDevManager)
    {
        int i = 0;
        for(; i < UVC_MAX_DEV_NUM; i++)
        {
            if(gpUvcDevManager->mUvcChnManager[i])
            {
                if(uvcInput_Destruct(gpUvcDevManager->mUvcChnManager[i]->mUvcDevName) != SUCCESS)
                {
                    aloge("can not destrcut % UVC", gpUvcDevManager->mUvcChnManager[i]->mUvcDevName);
                    return FAILURE;
                }
            }            
        }
        pthread_mutex_destroy(&gpUvcDevManager->mUvcDevManagerLock);
        
        free(gpUvcDevManager);
        gpUvcDevManager = NULL;
    }
    return SUCCESS;
}

UVC_CHN_MAP_S *uvcInput_CHN_MAP_S_Construct()
{
    UVC_CHN_MAP_S *pChnnel = (UVC_CHN_MAP_S *)malloc(sizeof(UVC_CHN_MAP_S));
    if(NULL == pChnnel)
    {
        aloge("fatal error! malloc fail [%s]", strerror(errno));
        return NULL;
    }
    memset(pChnnel, 0, sizeof *pChnnel);
    cdx_sem_init(&pChnnel->mSemCompCmd, 0);
    return pChnnel;
}

void uvcInput_CHN_MAP_S_Destruct(UVC_CHN_MAP_S *pChnnel)
{
    if(pChnnel)
    {
        if(pChnnel->mUvcComp)
        {
            aloge("fatal error!, the %d channel componet need free before!", pChnnel->mUvcChn);
            COMP_FreeHandle(pChnnel->mUvcComp);
            pChnnel->mUvcComp = NULL;
        }
        cdx_sem_deinit(&pChnnel->mSemCompCmd);
        free(pChnnel);
        //pChnnel = NULL;//ignore
    }
    
}

ERRORTYPE uvcInput_addChannel(UVC_DEV UvcDev, UVC_CHN_MAP_S *pChn)
{
    ERRORTYPE bResult = FAILURE;

    if(UvcDev && pChn)
    {
        pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
        UvcChnManager **ppUvcChnManager = NULL;
        if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
        {
            aloge("fatal error! can not found %s UVC", UvcDev);
            pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
            return bResult;
        }
        UvcChnManager *pUvcChnManager = *ppUvcChnManager;
        pChn->mUvcComp->SetConfig(pChn->mUvcComp, COMP_IndexVendorUvcSetDevInfo, (void *) *ppUvcChnManager);
        pthread_mutex_lock(&pUvcChnManager->mLock);
        list_add_tail(&pChn->mList, &pUvcChnManager->mChnList);
        pthread_mutex_unlock(&pUvcChnManager->mLock);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        bResult = SUCCESS;
    }
    return bResult;    
}

ERRORTYPE uvcInput_removeChannel(UVC_DEV UvcDev, UVC_CHN_MAP_S *pChn)
{
    ERRORTYPE bResult = FAILURE;
    
    if(UvcDev && pChn)
    {
        pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
        UvcChnManager **ppUvcChnManager = NULL;
        if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
        {
            aloge("fatal error! can not found %s UVC", UvcDev);
            pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
            return bResult;
        }
        UvcChnManager *pUvcChnManager = *ppUvcChnManager;
        pthread_mutex_lock(&pUvcChnManager->mLock);
        list_del(&pChn->mList);
        pthread_mutex_unlock(&pUvcChnManager->mLock);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        bResult = SUCCESS;
    }
    return bResult;
}


ERRORTYPE uvcInput_Open_Video(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;
    if(UvcDev)
    {
        int fd = open(UvcDev, O_RDWR, 0);
        if(fd < 0)
        {
            aloge("the open %d UVC Camera fail!", UvcDev);
            return bResult;
        }

        if(uvcInput_Construct(UvcDev, fd) != SUCCESS)
        {
            aloge("the %s UVC do not construct!", UvcDev);
            uvcInput_Destruct(UvcDev);
            return bResult;
        }
        bResult = SUCCESS;
    }
    return bResult;
}

ERRORTYPE uvcInput_Close_Video(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;
    if(UvcDev)
    {
        if(uvcInput_Destruct(UvcDev) != SUCCESS)
        {
            aloge("the %s UVC do not destruct, and it means can not close fd", UvcDev);
            return bResult;
        }
        
    }
    bResult = SUCCESS;
    return bResult;
}

ERRORTYPE uvcInput_SetDevEnable(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;
    
    if(UvcDev)
    {
        pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
        UvcChnManager **ppUvcChnManager = NULL;
        if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
        {
            aloge("fatal error! can not found %s UVC", UvcDev);
            pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
            return bResult;
        }
        UvcChnManager *pUvcChnManager = *ppUvcChnManager;
        if(0 == pUvcChnManager->mUvcDevEnable)
        {
            int uvc_fd = pUvcChnManager->mFd;
            int buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(ioctl(uvc_fd, VIDIOC_STREAMON, &buffer_type) < 0)
            {
                aloge("the %s UVC fail to start capture, %s", UvcDev, strerror(errno));
                pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
                return bResult;
            }
            
            pUvcChnManager->mUvcDevEnable = 1;
            if(0 == pUvcChnManager->mThread_uvc_capture)
            {
                pthread_create(&pUvcChnManager->mThread_uvc_capture, NULL, uvcInput_CapThread,(void *)pUvcChnManager);
            }
            bResult = SUCCESS;
        }
        else
        {
            alogd("Be careful! uvcDev[%s] already enable!", UvcDev);
            bResult = SUCCESS;
        }
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    }
    return bResult;
}

ERRORTYPE uvcInput_SetDevDisable(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;
    
    if(UvcDev)
    {
        pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
        UvcChnManager **ppUvcChnManager = NULL;
        if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
        {
            aloge("fatal error! can not found %s UVC", UvcDev);
            pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
            return bResult;
        }
        UvcChnManager *pUvcChnManager = *ppUvcChnManager;
        if(pUvcChnManager->mUvcDevEnable)
        {
            int uvc_fd = pUvcChnManager->mFd;
            int buffer_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(ioctl(uvc_fd, VIDIOC_STREAMOFF, &buffer_type) < 0)
            {
                aloge("the %s fail to stop capture, %s", UvcDev, strerror(errno));
                //pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
                //return bResult;
            }  
            
            pUvcChnManager->mUvcDevEnable = 0;

            //pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
            
            if(pUvcChnManager->mThread_uvc_capture)
            {
                pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
                
                void *status;
                pthread_join(pUvcChnManager->mThread_uvc_capture, &status);

                pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
                pUvcChnManager->mThread_uvc_capture = 0;                
            }
        }     
        else
        {
            alogd("Be careful! uvcDev[%s] already disable!", UvcDev);
        }
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        bResult = SUCCESS;
    }
    return bResult;  
}


ERRORTYPE uvcInput_GetDevAttr(UVC_DEV UvcDev, UVC_ATTR_S *pAttr)
{
    ERRORTYPE bResult = FAILURE;
    
    if(UvcDev && pAttr)
    {
        pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
        UvcChnManager **ppUvcChnManager = NULL;
        if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
        {
            aloge("fatal error! can not found %s UVC", UvcDev);
            pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
            return bResult;
        }
        UvcChnManager *pUvcChnManager = *ppUvcChnManager;
        *pAttr = pUvcChnManager->mUvcAttr;
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        bResult = SUCCESS;        
    }
    
    return bResult;
}


ERRORTYPE uvcInput_SetDevAttr(UVC_DEV UvcDev, UVC_ATTR_S *pAttr)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev || !pAttr)
    {
        //aloge("fatal error!!");
        return bResult;
    }

    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);//1+
    UvcChnManager **ppUvcChnManager = NULL;
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        aloge("fatal error! can not found %s UVC", UvcDev);
        goto error;
    }
    UvcChnManager *pUvcChnManager = *ppUvcChnManager;
    //pUvcChnManager->mUvcAttr = *pAttr;
    int uvc_fd = pUvcChnManager->mFd;
    struct v4l2_capability cap;
    unsigned int  capabilities;
    memset(&cap, 0, sizeof cap);
    if(ioctl(uvc_fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        aloge(" the %s UVC fail to ioctl VIDIOC_QUERYCAP", UvcDev);
        goto error;
    }
    capabilities = cap.device_caps ? : cap.capabilities;
    if(capabilities & V4L2_CAP_VIDEO_CAPTURE)
    {       
        alogd("the %s UVC support video capture!", UvcDev);        
    }
    else
    {
        aloge("the %s UVC not support video capture!", UvcDev);
        goto error;
    }

#ifdef SHOW_INFO_OF_UVC
    alogd("the %s UVC driver is %s", UvcDev, cap.driver);
    alogd("the %s UVC card is %s", UvcDev, cap.card);
    alogd("the %s UVC bus info is %s", UvcDev, cap.bus_info);
    alogd("the version is %d", cap.version);
#endif

    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int support_pixelformat = 0;
    int nMatchFormatNum = 0;
    while(ioctl(uvc_fd, VIDIOC_ENUM_FMT, &fmt) == 0)
    {
        fmt.index++;
        alogd("the %s UVC device can support %s capture pixelformat[0x%x]", UvcDev, fmt.description, fmt.pixelformat);  //UVC_NV12
        if(fmt.pixelformat == pAttr->mPixelformat)
        {
            alogd("Congratulation: the %s UVC support %s capture pixelformat", UvcDev, fmt.description);
            support_pixelformat = 1;
            nMatchFormatNum++;
            
            struct v4l2_format format;
            memset(&format, 0, sizeof format);
            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            format.fmt.pix.pixelformat = pAttr->mPixelformat;
            format.fmt.pix.width = pAttr->mUvcVideo_Width;
            format.fmt.pix.height = pAttr->mUvcVideo_Height;
            if(ioctl(uvc_fd, VIDIOC_TRY_FMT, &format) < 0)
            {
                aloge("the %s UVC unable to set type frame!", UvcDev);
                goto error;
            }
            format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(ioctl(uvc_fd, VIDIOC_S_FMT, &format) < 0)
            {
                aloge("the %s UVC fail to set frame format!", UvcDev);
                goto error;
            }
            
            if(ioctl(uvc_fd, VIDIOC_G_FMT, &format) < 0)
            {
                aloge("the %s UVC fail to get frame format!", UvcDev);    
                goto error;
            }
            if(format.fmt.pix.width != pAttr->mUvcVideo_Width || format.fmt.pix.height != pAttr->mUvcVideo_Height)
            {
                alogw("Be careful! you can not set %s UVC capture [%dx%d],and now they are [%dx%d]", 
                        UvcDev, pAttr->mUvcVideo_Width, pAttr->mUvcVideo_Height,
                        format.fmt.pix.width,  format.fmt.pix.height);
            }
            pUvcChnManager->mUvcAttr.mPixelformat =  format.fmt.pix.pixelformat;
            pUvcChnManager->mUvcAttr.mUvcVideo_Width =  format.fmt.pix.width;
            pUvcChnManager->mUvcAttr.mUvcVideo_Height = format.fmt.pix.height;
            //break;           
        }
    }
    if(0 == support_pixelformat)
    {
        aloge("fatal error! Unfortunation: the %s UVC do not support 0x%x pixelformat", UvcDev, pAttr->mPixelformat);
        goto error;
    }
    if(nMatchFormatNum > 1)
    {
        alogd("Be careful! format match count[%d] > 1", nMatchFormatNum);
    }
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof streamparm);
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(uvc_fd, VIDIOC_G_PARM, &streamparm) < 0)
    {
        aloge("the %s UVC failed to get v4l2_streamparm!", UvcDev);
        goto error;
    }
    
    int fps = (streamparm.parm.capture.timeperframe.denominator) / (streamparm.parm.capture.timeperframe.numerator);
    if(pAttr->mUvcVideo_Fps != 0 && pAttr->mUvcVideo_Fps != fps)
    {
        streamparm.parm.capture.timeperframe.denominator = pAttr->mUvcVideo_Fps;
        streamparm.parm.capture.timeperframe.numerator = 1;
        if(ioctl(uvc_fd, VIDIOC_S_PARM, &streamparm) < 0)
        {
            aloge("the %s UVC can not set fps as %d, and default as %d", UvcDev, pAttr->mUvcVideo_Fps, fps);
            pUvcChnManager->mUvcAttr.mUvcVideo_Fps = fps;
        }
        else
        {
            memset(&streamparm, 0, sizeof streamparm);
            streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(ioctl(uvc_fd, VIDIOC_G_PARM, &streamparm) < 0)
            {
                aloge("the %s UVC failed to get v4l2_streamparm!", UvcDev);
                pUvcChnManager->mUvcAttr.mUvcVideo_Fps = fps;
            }
            else
            {
                int new_fps = (streamparm.parm.capture.timeperframe.denominator) / (streamparm.parm.capture.timeperframe.numerator);
                if(new_fps == pAttr->mUvcVideo_Fps)
                {
                    pUvcChnManager->mUvcAttr.mUvcVideo_Fps = pAttr->mUvcVideo_Fps;
                }
                else
                {
                    aloge("why not can set %s UVC capturerate from %d to %d fps? maybe the device can not support,and now the rate is %d fps!", UvcDev, fps, pAttr->mUvcVideo_Fps, new_fps);
                    pUvcChnManager->mUvcAttr.mUvcVideo_Fps = new_fps;                    
                }
            }
            //pUvcChnManager->mUvcAttr.mUvcVideo_Fps = pAttr->mUvcVideo_Fps;
        }
    }
    else
    {
        pUvcChnManager->mUvcAttr.mUvcVideo_Fps = fps;            
    }
    
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof req);
    if(pAttr->mUvcVideo_BufCnt <= BUFFER_COUNT)
    {
        pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt = pAttr->mUvcVideo_BufCnt;
    }
    else
    {
        alogd("Be careful! user set bufCnt[%d] > max[%d], set to max!", pAttr->mUvcVideo_BufCnt, BUFFER_COUNT);
        pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt = BUFFER_COUNT;
    }
    req.count = pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(uvc_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        aloge("fatal error! the %s UVC reqbufs fail, %s", UvcDev, strerror(errno));
        goto error;
    }
    
    struct v4l2_buffer buffer;
    int bufferindex;
    for(bufferindex = 0; bufferindex < pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt; bufferindex++)
    {
        memset(&buffer, 0, sizeof buffer);
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = bufferindex;

        int bufferlength = 0;
        if(ioctl(uvc_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        {
            aloge("the %s UVC querybuf fail, %s", strerror(errno));
            pUvcChnManager->mBufferLength = bufferlength;
            goto error;
        }
        if(bufferindex != buffer.index)
        {
            aloge("fatal error! index is not same with appointment, [%d]!=[%d]", buffer.index, bufferindex);
        }
        pUvcChnManager->mUvcV4l2BufferInfo[buffer.index] = buffer;
        bufferlength = buffer.length;
        pUvcChnManager->mpVideoBuffer[bufferindex] = (unsigned char *)mmap(NULL, buffer.length, PROT_READ, MAP_SHARED,
                                                        uvc_fd, buffer.m.offset);
        if(MAP_FAILED == pUvcChnManager->mpVideoBuffer[bufferindex])
        {
            aloge("the %s UVC mmap fail, %s", UvcDev, strerror(errno));
            pUvcChnManager->mpVideoBuffer[bufferindex] = NULL;
            pUvcChnManager->mBufferLength = buffer.length;
            goto error;
        }
        if(ioctl(uvc_fd, VIDIOC_QBUF, &buffer) < 0)
        {
            aloge("the %s UVC qbuf fail, %s", UvcDev, strerror(errno));
            pUvcChnManager->mBufferLength = buffer.length;
            goto error;            
        }        
    }
    pUvcChnManager->mBufferLength = buffer.length;
    //because vo and venc need physical contiguous memory, so if uvc frame is raw type, 
    //we need create ion buffer to copy frames sent by uvc driver,
    if(UVC_H264 != pUvcChnManager->mUvcAttr.mPixelformat && UVC_MJPEG != pUvcChnManager->mUvcAttr.mPixelformat)
    {
#if 0
        unsigned char *pIonMem = ion_allocMem(buffer.length * BUFFER_COUNT * 3);
        if(!pIonMem)
        {
            aloge("error: ion_allocMem can not malloc %d bytes", pUvcChnManager->mBufferLength * BUFFER_COUNT);
            goto error;
        }
        unsigned int pIonMemPhyAddr = ion_getMemPhyAddr(pIonMem);
        for(bufferindex = 0; bufferindex < BUFFER_COUNT; bufferindex++)
        {
            pUvcChnManager->mUvcCaptureFrame[bufferindex].mId = bufferindex;
            pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[0] = pIonMem + 3 *bufferindex * pUvcChnManager->mBufferLength;
            pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[1] = pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[0] + pUvcChnManager->mBufferLength;
            pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[2] = pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[1] + pUvcChnManager->mBufferLength;

            pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[0] = pIonMemPhyAddr + 3 * bufferindex * pUvcChnManager->mBufferLength;
            pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[1] = pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[0] + pUvcChnManager->mBufferLength;
            pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[2] = pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[1] + pUvcChnManager->mBufferLength;
        }
#else
        for(bufferindex = 0; bufferindex < pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt; bufferindex++)
        {
            unsigned int alignLength = ALIGN(buffer.length, 1024) + 3*1024;
            IonAllocAttr stAllocAttr;
            memset(&stAllocAttr, 0, sizeof(IonAllocAttr));
            stAllocAttr.mLen = alignLength;
            stAllocAttr.mAlign = 0;
            stAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stAllocAttr.mbSupportCache = 0;
            unsigned char *pIonMem = ion_allocMem_extend(&stAllocAttr);
            if(!pIonMem)
            {
                aloge("error: ion_allocMem_extend can not malloc %d bytes", alignLength);
                goto error;
            }   
            unsigned int pIonMemPhyAddr = ion_getMemPhyAddr(pIonMem);
            pUvcChnManager->mUvcCaptureFrame[bufferindex].mId = bufferindex;
            if(UVC_NV12 == pUvcChnManager->mUvcAttr.mPixelformat)
            {
                unsigned int yAlignLength = ALIGN(pUvcChnManager->mUvcAttr.mUvcVideo_Width*pUvcChnManager->mUvcAttr.mUvcVideo_Height, 1024);
                unsigned int uAlignLength = ALIGN(pUvcChnManager->mUvcAttr.mUvcVideo_Width*pUvcChnManager->mUvcAttr.mUvcVideo_Height/2, 8);
                unsigned int uLength = pUvcChnManager->mUvcAttr.mUvcVideo_Width*pUvcChnManager->mUvcAttr.mUvcVideo_Height/2;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[0] = pIonMem;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[1] = pIonMem + yAlignLength;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[2] = pIonMem + yAlignLength + uAlignLength;

                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[0] = pIonMemPhyAddr;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[1] = pIonMemPhyAddr + yAlignLength;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[2] = pIonMemPhyAddr + yAlignLength + uAlignLength;
            }
            else if(UVC_YUY2 == pUvcChnManager->mUvcAttr.mPixelformat)
            {
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[0] = pIonMem;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[0] = pIonMemPhyAddr;
            }
            else
            {
                aloge("fatal error! unknown pixel format[0x%x]", pUvcChnManager->mUvcAttr.mPixelformat);
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mpVirAddr[0] = pIonMem;
                pUvcChnManager->mUvcCaptureFrame[bufferindex].VFrame.mPhyAddr[0] = pIonMemPhyAddr;
            }
        }
#endif
    }

    #ifdef SHOW_INFO_OF_UVC
    alogd("the %s UVC capture fixformat is %x", UvcDev, pUvcChnManager->mUvcAttr.mPixelformat);
    alogd("the %s UVC capture width is %d", UvcDev, pUvcChnManager->mUvcAttr.mUvcVideo_Width);
    alogd("the %s UVC capture height is %d", UvcDev, pUvcChnManager->mUvcAttr.mUvcVideo_Height);
    alogd("the %s UVC capture fps is %d", UvcDev, pUvcChnManager->mUvcAttr.mUvcVideo_Fps);
    alogd("the %s UVC capture bufCount is %d", UvcDev, pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt);
    #endif

    alogw("the %s UVC had init!", UvcDev);
    bResult = SUCCESS;
error:

    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    return bResult;
    
}

static ERRORTYPE uvcInput_WaiteBuffer(UVC_DEV UvcDev, int fd, int nMilliSec)
{
    ERRORTYPE bResult = FAILURE;
    
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = nMilliSec / 1000;
    tv.tv_usec = (nMilliSec % 1000) * 1000;

    int ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if(-1 == ret )
    {
        aloge("the %s UVC select error!", UvcDev);
        return bResult;
    }
    else if(0 == ret)
    {
        aloge("the %s UVC select timeout!", UvcDev);
        return bResult;
    }   
    bResult = SUCCESS;
    return bResult;    
}

static ERRORTYPE uvcInput_GetData(UVC_DEV UvcDev, VIDEO_FRAME_INFO_S *pstFrameInfo, int nMilliSec)
{
    ERRORTYPE bResult = FAILURE;

    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
    UvcChnManager **ppUvcChnManager = NULL;
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        aloge("fatal error! can not found %s UVC", UvcDev);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;
    }

    UvcChnManager *pUvcChnManager = *ppUvcChnManager;
    int uvc_fd = pUvcChnManager->mFd;

    if(uvcInput_WaiteBuffer(UvcDev, uvc_fd, nMilliSec) != SUCCESS)
    {
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;
    }
    
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof buffer);
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = BUFFER_COUNT;

    if(ioctl(uvc_fd, VIDIOC_DQBUF, &buffer) < 0)
    {
        aloge("the %s UVC get frame fail, %s", UvcDev, strerror(errno));
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;
    }
    //if(buffer.index < 0 || buffer.index >= BUFFER_COUNT)
    //{
    //    aloge("the %s UVC invalid buffer index: %d", UvcDev, buffer.index);
    //    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    //    return bResult;
    //}

    pstFrameInfo->mId = buffer.index;
    pstFrameInfo->VFrame.mpts = (int64_t)buffer.timestamp.tv_sec * 1000000LL + buffer.timestamp.tv_usec;
    pstFrameInfo->VFrame.mPixelFormat = MM_PIXEL_FORMAT_SINGLE;// single, mjpeg or h264 stream
    pstFrameInfo->VFrame.mWidth = pUvcChnManager->mUvcAttr.mUvcVideo_Width;
    pstFrameInfo->VFrame.mHeight= pUvcChnManager->mUvcAttr.mUvcVideo_Height;
    pstFrameInfo->VFrame.mpVirAddr[0] = pUvcChnManager->mpVideoBuffer[buffer.index];
    pstFrameInfo->VFrame.mStride[0] = buffer.bytesused;

    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    bResult = SUCCESS;
    return bResult;
    
}

static ERRORTYPE uvcInput_GetData2(UvcChnManager *pUvcChnManager, VIDEO_FRAME_INFO_S *pstFrameInfo, int nMilliSec)
{
    ERRORTYPE bResult = FAILURE;
    
    int uvc_fd = pUvcChnManager->mFd;
#if 1
    if(uvcInput_WaiteBuffer(pUvcChnManager->mUvcDevName, uvc_fd, nMilliSec) != SUCCESS)
    {
        return bResult;
    }
#endif
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof buffer);
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = BUFFER_COUNT;

    if(ioctl(uvc_fd, VIDIOC_DQBUF, &buffer) < 0)
    {
        if(pUvcChnManager->mUvcDevEnable) // check the uvc enable state, 
        {
            aloge("the %s UVC get frame fail, %s", pUvcChnManager->mUvcDevName, strerror(errno));
            usleep(200000);
        }
        return bResult;
    }

#if 0
    pstFrameInfo->mId = buffer.index;
    pstFrameInfo->VFrame.mpts = (int64_t)buffer.timestamp.tv_sec * 1000000LL + buffer.timestamp.tv_usec;
    pstFrameInfo->VFrame.mPixelFormat = MM_PIXEL_FORMAT_YUYV_PACKAGE_422;// note:
    pstFrameInfo->VFrame.mWidth = pUvcChnManager->mUvcAttr.mUvcVideo_Width;
    pstFrameInfo->VFrame.mHeight= pUvcChnManager->mUvcAttr.mUvcVideo_Height;
    pstFrameInfo->VFrame.mpVirAddr[0] = pUvcChnManager->mpVideoBuffer[buffer.index];
    pstFrameInfo->VFrame.mStride[0] = buffer.bytesused;
#endif
    if(buffer.index < BUFFER_COUNT)
    {
        if(V4L2_PIX_FMT_MJPEG == pUvcChnManager->mUvcAttr.mPixelformat || V4L2_PIX_FMT_H264 == pUvcChnManager->mUvcAttr.mPixelformat)
        {
            #if 0
            pUvcChnManager->mUvcCaptureFrame[buffer.index].mId = buffer.index;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpts = (int64_t)buffer.timestamp.tv_sec * 1000000LL + buffer.timestamp.tv_usec;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mPixelFormat = MM_PIXEL_FORMAT_BUTT;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mWidth = pUvcChnManager->mUvcAttr.mUvcVideo_Width;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mHeight = pUvcChnManager->mUvcAttr.mUvcVideo_Height;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mStride[0] = buffer.bytesused;
            memcpy(pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpVirAddr[0], pUvcChnManager->mpVideoBuffer[buffer.index], buffer.bytesused);
            *pstFrameInfo = pUvcChnManager->mUvcCaptureFrame[buffer.index];
            ion_flushCache(pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpVirAddr[0], 3 * pUvcChnManager->mBufferLength);
            #else
            pstFrameInfo->mId = buffer.index;
            pstFrameInfo->VFrame.mpts = (int64_t)buffer.timestamp.tv_sec * 1000000LL + buffer.timestamp.tv_usec;
            pstFrameInfo->VFrame.mPixelFormat = MM_PIXEL_FORMAT_BUTT;
            pstFrameInfo->VFrame.mWidth = pUvcChnManager->mUvcAttr.mUvcVideo_Width;
            pstFrameInfo->VFrame.mHeight = pUvcChnManager->mUvcAttr.mUvcVideo_Height;
            pstFrameInfo->VFrame.mStride[0] = buffer.bytesused;
            pstFrameInfo->VFrame.mpVirAddr[0] = pUvcChnManager->mpVideoBuffer[buffer.index];
            
            #endif
            bResult = SUCCESS;    
        }
        else if(V4L2_PIX_FMT_YUYV == pUvcChnManager->mUvcAttr.mPixelformat)
        {
            pUvcChnManager->mUvcCaptureFrame[buffer.index].mId = buffer.index;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpts = (int64_t)buffer.timestamp.tv_sec * 1000000LL + buffer.timestamp.tv_usec;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mPixelFormat = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(pUvcChnManager->mUvcAttr.mPixelformat); //MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mField = VIDEO_FIELD_FRAME;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mWidth = pUvcChnManager->mUvcAttr.mUvcVideo_Width;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mHeight = pUvcChnManager->mUvcAttr.mUvcVideo_Height;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mOffsetTop = 0;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mOffsetBottom = pUvcChnManager->mUvcAttr.mUvcVideo_Height;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mOffsetLeft = 0;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mOffsetRight = pUvcChnManager->mUvcAttr.mUvcVideo_Width;

            /*yuv422pa_yuv422sp(pUvcChnManager->mpVideoBuffer[buffer.index], 
                                pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpVirAddr[0], 
                                pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpVirAddr[1], 
                                pUvcChnManager->mUvcAttr.mUvcVideo_Width, 
                                pUvcChnManager->mUvcAttr.mUvcVideo_Height);
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mStride[0] = pUvcChnManager->mUvcAttr.mUvcVideo_Width * pUvcChnManager->mUvcAttr.mUvcVideo_Height;
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mStride[1] = pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mStride[0];
            */
            memcpy(pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpVirAddr[0], pUvcChnManager->mpVideoBuffer[buffer.index], pUvcChnManager->mBufferLength);
            pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mStride[0] = pUvcChnManager->mBufferLength;
            ion_flushCache(pUvcChnManager->mUvcCaptureFrame[buffer.index].VFrame.mpVirAddr[0], pUvcChnManager->mBufferLength);
            *pstFrameInfo = pUvcChnManager->mUvcCaptureFrame[buffer.index];
            
            bResult = SUCCESS; 
            
        }
        else if(V4L2_PIX_FMT_NV12 == pUvcChnManager->mUvcAttr.mPixelformat)
        {
            aloge("sorry, this version do not support the V4L2_PIX_FMT_NV12 for UVC!!!");   
        }
    }
    else
    {
        aloge("fatal error! why dqueue success, but index[%d] >=[%d]?", buffer.index, BUFFER_COUNT);
    }
    
    return bResult;
    
}

ERRORTYPE uvcInput_ReleaseData2(UvcChnManager *pUvcChnManager, int buffer_index)
{
    ERRORTYPE bResult = FAILURE;

    int uvc_fd = pUvcChnManager->mFd;
    if(ioctl(uvc_fd, VIDIOC_QBUF, &pUvcChnManager->mUvcV4l2BufferInfo[buffer_index]) < 0)
    {
        aloge("the %s UVC can not query %d buffer, %s", pUvcChnManager->mUvcDevName, buffer_index, strerror(errno));
        return bResult;
    }
    bResult = SUCCESS;
    return bResult;
}
static ERRORTYPE uvcInput_ReleaseData(UVC_DEV UvcDev, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    ERRORTYPE bResult = FAILURE;

    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
    UvcChnManager **ppUvcChnManager = NULL;
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        aloge("fatal error! can not found %s UVC", UvcDev);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;
    }

    bResult = uvcInput_ReleaseData2(*ppUvcChnManager, pstFrameInfo->mId);
    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    return bResult;
}


ERRORTYPE uvcInput_RefsReduceAndRleaseData2(UvcChnManager *pUvcChnManager, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    ERRORTYPE bResult = SUCCESS;
    
    pthread_mutex_lock(&pUvcChnManager->mRefsLock);
    if(pUvcChnManager->mRefs[pstFrameInfo->mId] <= 0)
    {
        aloge("fatal error! %s UVC, idx[%d]: ref=[%d] when reduce refs, check code!",
                            pUvcChnManager->mUvcDevName, pstFrameInfo->mId,
                                               pUvcChnManager->mRefs[pstFrameInfo->mId] );
    }
    pUvcChnManager->mRefs[pstFrameInfo->mId]--;
    int refs = pUvcChnManager->mRefs[pstFrameInfo->mId];

    if(0 == refs)
    {
        bResult = uvcInput_ReleaseData2(pUvcChnManager, pstFrameInfo->mId);
    }
    pthread_mutex_unlock(&pUvcChnManager->mRefsLock);
    return bResult;
    
}

ERRORTYPE uvcInput_RefsReduceAndRleaseData(UVC_DEV UvcDev, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    ERRORTYPE bResult = FAILURE;

    pthread_mutex_lock(&gpUvcDevManager->mUvcDevManagerLock);
    UvcChnManager **ppUvcChnManager = NULL;
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        aloge("fatal error! can not found %s UVC", UvcDev);
        pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
        return bResult;
    }

    bResult =  uvcInput_RefsReduceAndRleaseData2(*ppUvcChnManager, pstFrameInfo);
    pthread_mutex_unlock(&gpUvcDevManager->mUvcDevManagerLock);
    return bResult;
    
}


static void *uvcInput_CapThread(void *pThreadData)
{
    UvcChnManager *pUvcChnManager = (UvcChnManager *)pThreadData;
    if(NULL == pUvcChnManager)
    {
        aloge("fatal error, the UvcChnManager is NULL");
        return NULL;
    }
    VIDEO_FRAME_INFO_S	pstFrameInfo;
    int nMilliSec = 200;
    int num_buf = pUvcChnManager->mUvcAttr.mUvcVideo_BufCnt;
    int i, ret;
    prctl(PR_SET_NAME, "UVCCaptureThread", 0, 0, 0);
    alogw("loop uvcInput_CapThread the UVC name is %s , fd = %d", pUvcChnManager->mUvcDevName, pUvcChnManager->mFd);
    while(1)
    {
        if(0 == pUvcChnManager->mUvcDevEnable) //disable the UVC
        {
            while(1)
            {
                if(list_empty(&pUvcChnManager->mChnList))
                {
                    break;
                }
                usleep(100000);
                alogd("Virvi Com not exit !!!");
            }

            for(i = 0; i < num_buf; i++)
            {
                if(0 != pUvcChnManager->mRefs[i])
                {
                    alogd("fatal error! Virvi Com not return all yuv frame !!! ");
                    uvcInput_ReleaseData2(pUvcChnManager, i);
                    pUvcChnManager->mRefs[i] = 0;
                }
            }
            break;
        }

        memset(&pstFrameInfo, 0, sizeof pstFrameInfo);
        if(SUCCESS == uvcInput_GetData2(pUvcChnManager, &pstFrameInfo, nMilliSec))
        {   
            //alogw("get one frame[%d]!", pstFrameInfo.mId);
            UVC_CHN_MAP_S *pEntry;
            pthread_mutex_lock(&pUvcChnManager->mLock);

            if(0 == pUvcChnManager->mRefs[pstFrameInfo.mId])
            {
                if(list_empty(&pUvcChnManager->mChnList))
                {
                    aloge("the %s UVC, No Virvi Component, drop this one yuv data.", pUvcChnManager->mUvcDevName);
                    uvcInput_ReleaseData2(pUvcChnManager, pstFrameInfo.mId);
                    pthread_mutex_unlock(&pUvcChnManager->mLock);
                    continue;
                }                
                pthread_mutex_lock(&pUvcChnManager->mRefsLock);
                pUvcChnManager->mRefs[pstFrameInfo.mId]++;
                pthread_mutex_unlock(&pUvcChnManager->mRefsLock);

                list_for_each_entry(pEntry, &pUvcChnManager->mChnList, mList)
                {
                    COMP_BUFFERHEADERTYPE bufferHeader;
                    bufferHeader.nInputPortIndex = UVC_CHN_PORT_INDEX_NCOM_IN;
                    bufferHeader.pOutputPortPrivate = &pstFrameInfo;
                    pthread_mutex_lock(&pUvcChnManager->mRefsLock);
                    pUvcChnManager->mRefs[pstFrameInfo.mId]++;
                    pthread_mutex_unlock(&pUvcChnManager->mRefsLock);        
                    ret =pEntry->mUvcComp->EmptyThisBuffer(pEntry->mUvcComp, &bufferHeader);
                    if(ret != SUCCESS)
                    {
                        pthread_mutex_lock(&pUvcChnManager->mRefsLock);
                        pUvcChnManager->mRefs[pstFrameInfo.mId]--;
                        pthread_mutex_unlock(&pUvcChnManager->mRefsLock);        
                    }
                }
                //uvcInput_ReleaseData2(pUvcChnManager, pstFrameInfo.mId);
                uvcInput_RefsReduceAndRleaseData2(pUvcChnManager, &pstFrameInfo);
            }
            else
            {
                aloge("fatal error! the %s UVC buf not return, refs id = %d,buffer index = %d, drop this yuv data",
                                         pUvcChnManager->mUvcDevName, pUvcChnManager->mRefs[pstFrameInfo.mId],
                                                                                    pstFrameInfo.mId);
                uvcInput_ReleaseData2(pUvcChnManager, pstFrameInfo.mId);               
            }
            pthread_mutex_unlock(&pUvcChnManager->mLock);            
        }
        else
        {
            //usleep(40000);
            continue;
        }
    } 
    return NULL;
}

static ERRORTYPE set_uvc_control(int fd, int cmd_id, int set_value, const char *cmd_name)
{
    ERRORTYPE bResult = FAILURE;
   
    struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof queryctrl);
    if(uvc_query(fd, cmd_id, cmd_name, &queryctrl) != SUCCESS)
    {
        aloge("fatal error! the UVC maybe can not support %s set!", cmd_name);
        return bResult;
    }
    if(set_value > queryctrl.maximum && set_value < queryctrl.minimum)
    {
        aloge("fatal error!, the set %s value  %d had not in range[%d   %d]", cmd_name, set_value, queryctrl.minimum, queryctrl.maximum);
        return bResult;
    }

    struct v4l2_control control;
    memset(&control, 0, sizeof control);
    control.id = cmd_id;
    control.value = set_value;
    if(ioctl(fd, VIDIOC_S_CTRL, &control) < 0)
    {
        aloge("error! failed to set %s to %d, %s", cmd_name, set_value, strerror(errno));
        return bResult;
    }

    return SUCCESS;
    
}

static ERRORTYPE get_uvc_control(int fd, int cmd_id, int *get_value, const char *cmd_name)
{
    ERRORTYPE bResult = FAILURE;

     struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof queryctrl);
    if(uvc_query(fd, cmd_id, cmd_name, &queryctrl) != SUCCESS)
    {
        aloge("fatal error! this UVC camera do not support %s ", cmd_name);
        return bResult;
    }

    struct v4l2_control control;
    memset(&control, 0, sizeof control);
    control.id = cmd_id;
    if(ioctl(fd, VIDIOC_G_CTRL, &control) < 0)
    {
        aloge("error! failed to get %s, %s", cmd_name, strerror(errno));
        return bResult;
    }
    
    *get_value = control.value;    
    return SUCCESS;
    
}

ERRORTYPE uvcInput_SetControl(UVC_DEV UvcDev, int cmd_id, int set_value, const char *cmd_name)
{    
    UvcChnManager **ppUvcChnManager = NULL;
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        aloge("fatal error! can not found %s UVC", UvcDev);
        return FAILURE;
    }
    UvcChnManager *pUvcChnManager = *ppUvcChnManager;
    int uvc_fd = pUvcChnManager->mFd;

    return set_uvc_control(uvc_fd, cmd_id, set_value, cmd_name);      
}

ERRORTYPE uvcInput_GetControl(UVC_DEV UvcDev, int cmd_id, int *get_value, const char *cmd_name)
{
     UvcChnManager **ppUvcChnManager = NULL;
    if(uvcInput_SearchExitDev(UvcDev, &ppUvcChnManager) != SUCCESS)
    {
        aloge("fatal error! can not found %s UVC", UvcDev);
        return FAILURE;
    }
    UvcChnManager *pUvcChnManager = *ppUvcChnManager;
    int uvc_fd = pUvcChnManager->mFd;

    return get_uvc_control(uvc_fd, cmd_id, get_value, cmd_name);    
}



