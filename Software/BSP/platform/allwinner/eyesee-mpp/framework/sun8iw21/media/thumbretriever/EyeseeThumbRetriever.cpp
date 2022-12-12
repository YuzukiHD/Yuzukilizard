/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeThumRetriever.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/10/27
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeThumbRetriever"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <EyeseeThumbRetriever.h>
#include <plat_log.h>
#include <plat_type.h>
#include <mpi_sys.h>
#include <mpi_demux.h>
#include <mpi_venc.h>
#include <mpi_vdec.h>
#include <mpi_clock.h>
#include <Clock_Component.h>

static bool IsJpegSource(int fd)
{
    char szJpegContent[10] = {0};
    bool bJpegSource = true;

    char szExif[4] = {0x45, 0x78, 0x69, 0x66};
    char szJFIF[4] = {0x4A, 0x46, 0x49, 0x46};

    struct stat JpegStat;
    if (fstat(fd, &JpegStat) != 0 || JpegStat.st_size < 10)
    {
        return false;
    }

    lseek(fd, 0, SEEK_SET);
    read(fd, szJpegContent, 10);
    lseek(fd, 0, SEEK_SET);

    // Check SOI
    if (szJpegContent[0] != 0xFF || szJpegContent[1] != 0xD8)
    {
        bJpegSource = false;
    }

    if (szJpegContent[2] == 0xFF && szJpegContent[3] == 0xE0)
    {// JFIF
        if (memcmp(&szJpegContent[6], szJFIF, 4) != 0)
        {
            bJpegSource = false;
        }
    }
    else if (szJpegContent[2] == 0xFF && szJpegContent[3] == 0xE1)
    {// Exif
        if (memcmp(&szJpegContent[6], szExif, 4) != 0)
        {
            bJpegSource = false;
        }
    }
    else if (szJpegContent[2] == 0xFF && szJpegContent[3] == 0xDB)
    {// no header
        alogv("this jpg file doesn't have header");
    }
    else
    {
        bJpegSource = false;
    }

    return bJpegSource;
}

static bool GetJpegInfo(int fd, int* pWidth, int* pHeight, int* pFileSize)
{
    *pWidth = 0;
    *pHeight = 0;
    *pFileSize = 0;

    char cJpegFlag;
    char szJpegContent[512];

    struct stat JpegStat;
    if (fstat(fd, &JpegStat) != 0 || JpegStat.st_size < 4)
    {
        return false;
    }
    *pFileSize = JpegStat.st_size;

    int readlen = 0;
    lseek(fd, 0, SEEK_SET);
    while (1)
    {
        if (1 != read(fd, &cJpegFlag, 1))
        {//eof
            return false;
        }

        // Sync 0xFF
        if (cJpegFlag != 0xFF)
        {
            continue;
        }

        if (1 != read(fd, &cJpegFlag, 1))
        {//eof
            return false;
        }

        if (cJpegFlag == 0xD8 || cJpegFlag == 0xD9 || cJpegFlag == 0x00)
        {// head, eof or data
            continue;
        }
        else if (cJpegFlag == 0xE0 || cJpegFlag == 0xE1)
        {
            read(fd, &szJpegContent, 2);
            int len = szJpegContent[0] * 256 + szJpegContent[1];
            lseek(fd, len, SEEK_CUR);
            continue;
        }
        else if (cJpegFlag == 0xC4)
        {// Huffman Table
            return false;
        }
        else if (cJpegFlag == 0xC0 || cJpegFlag == 0xC2)
        {// SOF0 or SOF2, Start of Frame
            int iSOF_len = 0;
            if (2 == read(fd, szJpegContent, 2))
            {
                iSOF_len = szJpegContent[0] * 256 + szJpegContent[1];
            }
            else
            {
                return false;
            }

            if (iSOF_len - 2 == read(fd, szJpegContent, iSOF_len - 2))
            {
                *pHeight = szJpegContent[1] * 256 + szJpegContent[2];
                *pWidth = szJpegContent[3] * 256 + szJpegContent[4];
            }
            else
            {
                return false;
            }
            return true;
        }
        else
        {
            read(fd, &cJpegFlag, 1);
        }
    }
}

namespace EyeseeLinux {

EyeseeThumbRetriever::EyeseeThumbRetriever()
{
   mDmxChn = MM_INVALID_CHN;
   mVdecChn = MM_INVALID_CHN;
   mVecChn = MM_INVALID_CHN;
   mClockChn = MM_INVALID_CHN;

   bJpegSource = false;
   mJpegWidth = 0;
   mJpegHeight = 0;
   mSeekStartPosition = 0;

   memset(&mDmxChnAttr, 0, sizeof(DEMUX_CHN_ATTR_S));
   mDmxChnAttr.mFd = -1;

   mCurrentState = MEDIA_THUMB_IDLE;
}

EyeseeThumbRetriever::~EyeseeThumbRetriever()
{

}

status_t EyeseeThumbRetriever::setDataSource(int fd, int64_t offset, int64_t length)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState & MEDIA_THUMB_IDLE || mCurrentState == MEDIA_THUMB_STATE_ERROR))
    {
        aloge("called in wrong state %d", mCurrentState);
        return INVALID_OPERATION;
    }

    memset(&mDmxChnAttr, 0, sizeof(mDmxChnAttr));

    // Check JPEG file
    if (IsJpegSource(fd))
    {
        bJpegSource = true;
        mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
        mDmxChnAttr.mSourceType = SOURCETYPE_FD;
        mDmxChnAttr.mSourceUrl = NULL;
        mDmxChnAttr.mFd = dup(fd);
        mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_AUDIO_TRACK;
        mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_SUBTITLE_TRACK;

        mCurrentState = MEDIA_THUMB_INITIALIZED;
        return NO_ERROR;
    }

    ERRORTYPE ret;
    bool nSuccessFlag = false;

    mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    mDmxChnAttr.mSourceUrl = NULL;
    mDmxChnAttr.mFd = dup(fd);
    mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_AUDIO_TRACK;
    mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_SUBTITLE_TRACK;
    mDmxChn = 0;
    while(mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(mDmxChn, &mDmxChnAttr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", mDmxChn);
            break;
        }
        else if(ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", mDmxChn);
            mDmxChn++;
        }
        else if(ERR_DEMUX_FILE_EXCEPTION == ret)
        {
            aloge("demux detect media file exception!");
            if(0 == mDmxChnAttr.mFd)
            {
                alogd("Be careful! mFd == 0!");
            }
            close(mDmxChnAttr.mFd);
            mDmxChnAttr.mFd = -1;
            return BAD_FILE;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", mDmxChn, ret);
            break;
        }
    }
    if(false == nSuccessFlag)
    {
        mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        if(0 == mDmxChnAttr.mFd)
        {
            alogd("Be careful! mFd == 0!");
        }
        close(mDmxChnAttr.mFd);
        mDmxChnAttr.mFd = -1;
        return UNKNOWN_ERROR;
    }

    mCurrentState = MEDIA_THUMB_INITIALIZED;
    return NO_ERROR;

}

status_t EyeseeThumbRetriever::setDataSource(const char *url)
{
    alogv("setDataSource, path=%s", url);
    status_t opStatus;
    if (access(url, F_OK) == 0)
    {
        int fd = open(url, O_RDONLY);
        if (fd < 0)
        {
            aloge("Failed to open file %s(%s)", url, strerror(errno));
            return UNKNOWN_ERROR;
        }
        opStatus = setDataSource(fd, 0, 0x7ffffffffffffffL);
        close(fd);
    }
    else
    {
        aloge("fatal error! open file path[%s] fail!", url);
        opStatus = INVALID_OPERATION;
    }

    return opStatus;
}


std::shared_ptr<CMediaMemory> EyeseeThumbRetriever::getThmPicInfo(char *p_fname)
{
    int tmp_ret = 0;
    int fd = -1;
    std::shared_ptr<CMediaMemory> spJpegBuf;
    aloge("thum_pic:%s",p_fname);    
    
    fd = open(p_fname,O_RDONLY);
    if(-1 == fd)
    {
        aloge("oen_file_failed");
        return NULL;
    }

    unsigned int atom_size = 0;
    char atom_type[5] = {0}; 

    tmp_ret = read(fd,&atom_size,sizeof(unsigned int));
    if(tmp_ret != sizeof(unsigned int))
    {
        aloge("get_thm_pic_read_failed,r:%d,t:%u",tmp_ret,sizeof(unsigned int));
    }
    atom_size = (atom_size&0xff000000)>>24|((atom_size&0x00ff0000)>>16)<<8| 
                    ((atom_size&0x0000ff00)>>8)<<16 | (atom_size&0xff)<<24;
    tmp_ret = read(fd,atom_type,4);
    if(tmp_ret != 4)
    {
        aloge("get_thm_pic_read_atom_type_fail!");
    }
    atom_type[4] = '\0';
    if(strcmp(atom_type,"ftyp"))
    {
        aloge("get_thm_pic_chk_file_type_fail,c:%s",atom_type);

        if(-1 != fd)
        {
            close(fd);
        }
        return NULL;
    }
    else
    {
        lseek(fd,atom_size-8,SEEK_CUR);
    }
    while(1)
    {
        tmp_ret = read(fd,&atom_size,sizeof(unsigned int));
        if(tmp_ret != sizeof(unsigned int))
        {
            aloge("get_thm_pic_read_failed,r:%d,t:%u",tmp_ret,sizeof(unsigned int));
        }

        atom_size = (atom_size&0xff000000)>>24|((atom_size&0x00ff0000)>>16)<<8| 
                        ((atom_size&0x0000ff00)>>8)<<16 | (atom_size&0xff)<<24;
        tmp_ret = read(fd,atom_type,4);
        if(tmp_ret != 4)
        {
            aloge("get_thm_pic_read_atom_type_fail!");
        }
        atom_type[4] = '\0';
        if(!strcmp(atom_type,"thm "))
        {
            spJpegBuf = std::make_shared<CMediaMemory>(atom_size-8);
            char *ptr = (char *)spJpegBuf->getPointer();

            tmp_ret = read(fd,ptr,atom_size-8);
            if((unsigned int)tmp_ret != atom_size-8)
            {
                aloge("get_thm_pic_read_data_fail,r:%d,t:%d",tmp_ret,atom_size-4);
            }

            if(-1 != fd)
            {
                close(fd);
            }
            return spJpegBuf;
        }
        else
        {
            tmp_ret = lseek(fd,atom_size-8,SEEK_CUR);
            if(-1 == tmp_ret)
            {
                aloge("get_thm_pic_seek_failed,t:%s,s:%d",atom_type,atom_size);

                if(-1 != fd)
                {
                    close(fd);
                }
                return NULL;
            }
        }
    }
}

status_t EyeseeThumbRetriever::getMediaInfo(DEMUX_MEDIA_INFO_S * pMediaInfo)
{
    Mutex::Autolock _l(mLock);
    memset(pMediaInfo, 0, sizeof(DEMUX_MEDIA_INFO_S));

    if (mCurrentState != MEDIA_THUMB_INITIALIZED)
    {// make sure data source is ready
        return NO_INIT;
    }

    if (bJpegSource)
    {
        int iWidth, iHeight, iFileSize;
        if (!GetJpegInfo(mDmxChnAttr.mFd, &iWidth, &iHeight, &iFileSize))
        {
            aloge("fatal error! couldn't get jpeg file info");
            return UNKNOWN_ERROR;
        }

        pMediaInfo->mFileSize   = iFileSize;
        pMediaInfo->mDuration   = 0;
        pMediaInfo->mVideoIndex = 0;
        pMediaInfo->mVideoNum   = 1;
        pMediaInfo->mVideoStreamInfo[0].mCodecType   = PT_JPEG;
        pMediaInfo->mVideoStreamInfo[0].mWidth       = iWidth;
        pMediaInfo->mVideoStreamInfo[0].mHeight      = iHeight;
        pMediaInfo->mVideoStreamInfo[0].mFrameRate   = 0;
        pMediaInfo->mVideoStreamInfo[0].mAvgBitsRate = 0;
        pMediaInfo->mVideoStreamInfo[0].mMaxBitsRate = 0;
        return NO_ERROR;
    }

    ERRORTYPE ret = AW_MPI_DEMUX_GetMediaInfo(mDmxChn, pMediaInfo);
    if (ret == SUCCESS)
    {
        if(  (pMediaInfo->mVideoNum > 0 && pMediaInfo->mVideoIndex >= pMediaInfo->mVideoNum)
          || (pMediaInfo->mAudioNum > 0 && pMediaInfo->mAudioIndex >= pMediaInfo->mAudioNum)
          || (pMediaInfo->mSubtitleNum > 0 && pMediaInfo->mSubtitleIndex >= pMediaInfo->mSubtitleNum)
          )
        {
            alogd("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
                pMediaInfo->mVideoNum, pMediaInfo->mVideoIndex, pMediaInfo->mAudioNum, pMediaInfo->mAudioIndex, pMediaInfo->mSubtitleNum, pMediaInfo->mSubtitleIndex);
            return UNKNOWN_ERROR;
        }
        return NO_ERROR;
    }
    else
    {
        return UNKNOWN_ERROR;
    }
}


std::shared_ptr<CMediaMemory> EyeseeThumbRetriever::getJpegAtTime(int64_t timeUs, int reqWidth, int reqHeight)
{
    Mutex::Autolock _l(mLock);

    if (bJpegSource)
    {
        aloge("fatal error! JPEG source don't support thumb retrive");
        return NULL;
    }

    mJpegWidth = reqWidth;
    mJpegHeight = reqHeight;
    mSeekStartPosition = timeUs / 1000; //ms
    if(!(mJpegWidth>0 && mJpegHeight>0))
    {
        aloge("fatal error! wrong jpegSize[%dx%d]", mJpegWidth, mJpegHeight);
        return NULL;
    }
    std::shared_ptr<CMediaMemory> spJpegBuf;
    prepare();

    seekTo();

    start();

    if(mVecChn >= 0)
    {
        VENC_STREAM_S JpegStream;
        VENC_PACK_S Jpeg_pack;
        JpegStream.mPackCount = 1;
        JpegStream.mpPack = &Jpeg_pack;

        ERRORTYPE ret = AW_MPI_VENC_GetStream(mVecChn, &JpegStream, 500);
        if(SUCCESS == ret)
        {
            int JpegSize = JpegStream.mpPack[0].mLen0 + JpegStream.mpPack[0].mLen1;
            spJpegBuf = std::make_shared<CMediaMemory>(JpegSize);
            char *ptr = (char *)spJpegBuf->getPointer();
            memcpy(ptr, JpegStream.mpPack[0].mpAddr0, JpegStream.mpPack[0].mLen0);
            ptr += JpegStream.mpPack[0].mLen0;
            memcpy(ptr, JpegStream.mpPack[0].mpAddr1, JpegStream.mpPack[0].mLen1);

            if(AW_MPI_VENC_ReleaseStream(mVecChn, &JpegStream) != SUCCESS)
            {
                aloge("jpeg stream data return fail!");
            }
        }
        else
        {
            aloge("get jpeg stream data fail!");
        }
    }
    else
    {
        aloge("fatal error! jpeg vencode channel do not creat!!!");
    }

    stop();

    return spJpegBuf;
}

status_t EyeseeThumbRetriever::reset()
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState & (MEDIA_THUMB_INITIALIZED | MEDIA_THUMB_STOPPED)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    if(mVecChn >= 0)
    {
        AW_MPI_VENC_DestroyChn(mVecChn);
        mVecChn = MM_INVALID_CHN;
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_DestroyChn(mVdecChn);
        mVdecChn = MM_INVALID_CHN;
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(mDmxChn);
        mDmxChn = MM_INVALID_CHN;
    }
    if(mDmxChnAttr.mFd >= 0)
    {
        if(0 == mDmxChnAttr.mFd)
        {
            alogd("Be careful! mFd == 0!");
        }
        close(mDmxChnAttr.mFd);
        mDmxChnAttr.mFd = -1;
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(mClockChn);
        mDmxChn = MM_INVALID_CHN;
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(mClockChn);
        mClockChn = MM_INVALID_CHN;
    }
    mCurrentState = MEDIA_THUMB_IDLE;
    bJpegSource = false;

    return NO_ERROR;
}

status_t EyeseeThumbRetriever::prepare()
{
    status_t result = NO_ERROR;
    if(!(mCurrentState & (MEDIA_THUMB_INITIALIZED | MEDIA_THUMB_STOPPED)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    mClockChnAttr.nWaitMask = 0;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;
    if(AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return UNKNOWN_ERROR;
    }
    if((DemuxMediaInfo.mVideoNum > 0 && DemuxMediaInfo.mVideoIndex >= DemuxMediaInfo.mVideoNum)
        || (DemuxMediaInfo.mAudioNum > 0 && DemuxMediaInfo.mAudioIndex >= DemuxMediaInfo.mAudioNum)
        || (DemuxMediaInfo.mSubtitleNum > 0 && DemuxMediaInfo.mSubtitleIndex >= DemuxMediaInfo.mSubtitleNum)
        )
    {
        alogd("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
            DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex, DemuxMediaInfo.mAudioNum, DemuxMediaInfo.mAudioIndex, DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return UNKNOWN_ERROR;
    }

    ERRORTYPE ret;
    if(DemuxMediaInfo.mVideoNum > 0 && !(mDmxChnAttr.mDemuxDisableTrack & DEMUX_DISABLE_VIDEO_TRACK))
    {
        DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo = &DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex];

        memset(&mVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
        mVdecChnAttr.mType = pStreamInfo->mCodecType;
        mVdecChnAttr.mPicWidth = 0;
        mVdecChnAttr.mPicHeight = 0;
        mVdecChnAttr.mInitRotation = (ROTATE_E)0;
        mVdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
        mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;

        bool nSuccessFlag = false;
        mVdecChn = 0;
        while(mVdecChn < VDEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VDEC_CreateChn(mVdecChn, &mVdecChnAttr);
            if(SUCCESS == ret)
            {
                nSuccessFlag = true;
                alogd("create vdec channel[%d] success!", mVdecChn);
                break;
            }
            else if(ERR_VDEC_EXIST == ret)
            {
                alogd("vdec channel[%d] is exist, find next!", mVdecChn);
                mVdecChn++;
            }
            else
            {
                alogd("error, create vdec channel[%d] ret[0x%x]!", mVdecChn, ret);
                break;
            }
        }
        if(!nSuccessFlag)
        {
            mVdecChn = MM_INVALID_CHN;
            aloge("fatal error! create vdec channel failed");
            result = UNKNOWN_ERROR;
            goto _err0;
        }

        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        MPP_CHN_S VdecChn{MOD_ID_VDEC, 0, mVdecChn};
        AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
        mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO;
    }

    if(mJpegWidth > 0 && mJpegHeight > 0 && mVdecChn >= 0)
    {
        memset(&mVecChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

        unsigned int mPictureNum = 1;
        unsigned int minVbvBufSize = mJpegWidth * mJpegHeight * 3/2;
        unsigned int vbvThreshSize = mJpegWidth*mJpegHeight;
        unsigned int vbvBufSize = (mJpegWidth * mJpegHeight * 3/2 /10 * mPictureNum) + vbvThreshSize;
        if(vbvBufSize < minVbvBufSize)
        {
            vbvBufSize = minVbvBufSize;
        }
        if(vbvBufSize > 16*1024*1024)
        {
            alogd("Be careful! vbvSize[%d]MB is too large, decrease to threshSize[%d]MB + 1MB", vbvBufSize/(1024*1024), vbvThreshSize/(1024*1024));
            vbvBufSize = vbvThreshSize + 1*1024*1024;
        }

        mVecChnAttr.VeAttr.Type = PT_JPEG;
        mVecChnAttr.VeAttr.AttrJpeg.MaxPicWidth = 0;
        mVecChnAttr.VeAttr.AttrJpeg.MaxPicHeight = 0;
        mVecChnAttr.VeAttr.AttrJpeg.BufSize = ((vbvBufSize + 1023) >> 10) << 10;
        mVecChnAttr.VeAttr.AttrJpeg.mThreshSize = vbvThreshSize;
        mVecChnAttr.VeAttr.AttrJpeg.bByFrame = TRUE;
        mVecChnAttr.VeAttr.AttrJpeg.PicWidth = mJpegWidth;
        mVecChnAttr.VeAttr.AttrJpeg.PicHeight = mJpegHeight;
        mVecChnAttr.VeAttr.AttrJpeg.bSupportDCF = FALSE;
        mVecChnAttr.VeAttr.MaxKeyInterval = 1;
        mVecChnAttr.VeAttr.SrcPicWidth = ALIGN(DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mWidth, 32);
        mVecChnAttr.VeAttr.SrcPicHeight = ALIGN(DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mHeight, 32);
        mVecChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
        mVecChnAttr.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

        ERRORTYPE ret;
        bool bSuccessFlag = false;
        mVecChn = 0;
        while(mVecChn < VENC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VENC_CreateChn(mVecChn, &mVecChnAttr);
            if(SUCCESS == ret)
            {
                bSuccessFlag = true;
                alogd("create venc channel[%d] success!", mVecChn);
                break;
            }
            else if(ERR_VENC_EXIST == ret)
            {
                alogd("venc channel[%d] is exist, find next!", mVecChn);
                mVecChn++;
            }
            else
            {
                alogd("create venc channel[%d] ret[0x%x], find next!",  mVecChn, ret);
                break;
            }
        }
        if(!bSuccessFlag)
        {
           mVecChn = MM_INVALID_CHN;
           aloge("fatal error! create venc channel fail!");
           result = UNKNOWN_ERROR;
           goto _err0;
        }

        VENC_CROP_CFG_S stVencCrop;
        memset(&stVencCrop, 0, sizeof(VENC_CROP_CFG_S));
        stVencCrop.bEnable = 1;
        stVencCrop.Rect.X = 0;
        stVencCrop.Rect.Y = 0;
        stVencCrop.Rect.Width = DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mWidth;
        stVencCrop.Rect.Height = DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mHeight;
        AW_MPI_VENC_SetCrop(mVecChn, &stVencCrop);

        MPP_CHN_S VdecChn{MOD_ID_VDEC, 0, mVdecChn};
        MPP_CHN_S VencChn{MOD_ID_VENC, 0, mVecChn};
        AW_MPI_SYS_Bind(&VdecChn, &VencChn);

    }

    if(mVdecChn >=0 && mVecChn >= 0)
    {
        bool bSuccessFlag = false;
        mClockChn = 0;
        while(mClockChn < CLOCK_MAX_CHN_NUM)
        {
            ret = AW_MPI_CLOCK_CreateChn(mClockChn, &mClockChnAttr);
            if(SUCCESS == ret)
            {
                bSuccessFlag = true;
                alogd("creat clock channel[%d] success!", mClockChn);
                break;
            }
            else if(ERR_CLOCK_EXIST == ret)
            {
                alogd("clock channel[%d] is exist, find next!", mClockChn);
                ++mClockChn;
            }
            else
            {
                alogd("creat clock channel[%d] ret[0x%x]", mClockChn, ret);
                break;
            }
        }

        if(!bSuccessFlag)
        {
            mClockChn = MM_INVALID_CHN;
            aloge("fatal error! create clock channel fail!");
            result = UNKNOWN_ERROR;
            goto _err0;
        }

        MPP_CHN_S ClockChn{MOD_ID_CLOCK, 0, mClockChn};
        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
    }

    mCurrentState = MEDIA_THUMB_PREPARED;

_err0:
    return result;

}

status_t EyeseeThumbRetriever::start()
{
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & MEDIA_THUMB_PREPARED))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    if(mVecChn >= 0)
    {
        VENC_RECV_PIC_PARAM_S  RecvParam;
        RecvParam.mRecvPicNum = 1;
        AW_MPI_VENC_StartRecvPicEx(mVecChn, &RecvParam);
    }
    if(mVdecChn >= 0)
    {
        //AW_MPI_VDEC_StartRecvStream(mVdecChn);
        VDEC_DECODE_FRAME_PARAM_S  DecodeParam;
        DecodeParam.mDecodeFrameNum = 1;
        AW_MPI_VDEC_StartRecvStreamEx(mVdecChn, &DecodeParam);
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Start(mDmxChn);
    }

    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Start(mClockChn);
    }

    mCurrentState = MEDIA_THUMB_STARTED;

    return opStatus;
}

status_t EyeseeThumbRetriever::stop()
{
    status_t opStatus = NO_ERROR;
    if(mCurrentState & MEDIA_THUMB_STOPPED)
    {
        alogv("already stopped");
        return SUCCESS;
    }
    if(!(mCurrentState & (MEDIA_THUMB_PREPARED|MEDIA_THUMB_STARTED)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Stop(mClockChn);
    }

    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Stop(mDmxChn);
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(mVdecChn);
    }
    if(mVecChn >= 0)
    {
        AW_MPI_VENC_StopRecvPic(mVecChn);
    }

    if(mVecChn >= 0)
    {
        AW_MPI_VENC_DestroyChn(mVecChn);
        mVecChn = MM_INVALID_CHN;
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_DestroyChn(mVdecChn);
        mVdecChn = MM_INVALID_CHN;
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(mDmxChn);
        mDmxChn = MM_INVALID_CHN;
    }
    if(mDmxChnAttr.mFd >= 0)
    {
        if(0 == mDmxChnAttr.mFd)
        {
            alogd("Be careful! mFd == 0!");
        }
        close(mDmxChnAttr.mFd);
        mDmxChnAttr.mFd = -1;
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(mClockChn);
        mClockChn = MM_INVALID_CHN;
    }
        

    mCurrentState = MEDIA_THUMB_STOPPED;

    return opStatus;
}

status_t EyeseeThumbRetriever::seekTo()
{
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & MEDIA_THUMB_PREPARED))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mSeekStartPosition > 0 && mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Seek(mDmxChn, mSeekStartPosition);
        alogd("the demux had set %d ms!", mSeekStartPosition);
    }
    return opStatus;
}

};

