/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxVideoResizeStream.c
* Description : video resize stream interface
* History :
*/

#ifndef _LARGEFILE64_SOURCE
//* defined this macro for using flag O_LARGEFILE in open() method, see 'man 2 open'
#define _LARGEFILE64_SOURCE
#endif
#define _FILE_OFFSET_BITS 64

#include <CdxStream.h>
#include <CdxAtomic.h>
#include <CdxMemory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define DEFAULT_PROBE_DATA_LEN (1024 * 128)

struct CdxVideoResizeStreamImplS
{
    CdxStreamT base;
//    cdx_atomic_t ref;
    cdx_atomic_t state;
    CdxStreamProbeDataT probeData;
    cdx_int32 ioErr;
    cdx_char*   buffer;
    char *filePath;
    unsigned char* pWritePtr;
    unsigned char* pReadPtr;
    unsigned int validDataSize;
    unsigned int validBufSize;
    unsigned int bufferSize;
    unsigned char*bufferEnd;
    unsigned char eosFlag;
    pthread_mutex_t mutex;
};

enum FileStreamStateE
{
    FILE_STREAM_IDLE = 0x00L,
    FILE_STREAM_READING = 0x01L,
    FILE_STREAM_SEEKING = 0x02L,
    FILE_STREAM_CLOSING = 0x03L,
};

#define VIDEORESIZE_STREAM_SCHEME "videoResize://"

#if 0

#define FILE_STREAM_SCHEME "file://"
#define FD_STREAM_SCHEME "fd://"
#define DEFAULT_PROBE_DATA_LEN (1024 * 128)

#define fopen64(uri) open(uri, O_LARGEFILE)

#define fseek64(fd, offset, whence) lseek64(fd, offset, whence)

#define ftell64(fd) lseek64(fd, 0, SEEK_CUR)

#define fread64(fd, buf, len) read(fd, buf, len)

//#define feof64(fd) lseek64(fd, 0, SEEK_CUR)

#define fclose64(fd) close(fd)

enum FileStreamStateE
{
    FILE_STREAM_IDLE = 0x00L,
    FILE_STREAM_READING = 0x01L,
    FILE_STREAM_SEEKING = 0x02L,
    FILE_STREAM_CLOSING = 0x03L,
};

/*fmt: "file://xxx" */
struct CdxVideoResizeStreamImplS
{
    CdxStreamT base;
//    cdx_atomic_t ref;
    cdx_atomic_t state;
    CdxStreamProbeDataT probeData;
    cdx_int32 ioErr;

    int fd;
    cdx_int64 offset;
    cdx_int64 size;

    /*when datasource uri is fd, then will try to get the absolute path like "/mnt/..." */
    char *redriectPath;
    char *filePath;
};

static char *FdToFilepath(const int fd)
{
    char fdInProc[1024] = {0};
    char filePath[1024] = {0};
    int ret;
    CDX_LOG_CHECK(fd > 0, "Are u kidding? fd(%d)", fd);

    snprintf(fdInProc, sizeof(fdInProc), "/proc/self/fd/%d", fd);
    strcpy(filePath, "file://");
    ret = readlink(fdInProc, filePath + 7, 1024 - 1 - 7);
    if (ret == -1)
    {
        CDX_LOGE("readlink failure, errno(%d)", errno);
        return NULL;
    }

    return strdup(filePath);
}

static inline cdx_int32 WaitIdleAndSetState(cdx_atomic_t *state, cdx_ssize val)
{
    while (!CdxAtomicCAS(state, FILE_STREAM_IDLE, val))
    {
        if (CdxAtomicRead(state) == FILE_STREAM_CLOSING)
        {
            CDX_LOGW("file is closing.");
            return CDX_FAILURE;
        }
    }
    return CDX_SUCCESS;
}

static CdxStreamProbeDataT *__FileStreamGetProbeData(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    return &impl->probeData;
}

static cdx_int32 __FileStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    struct CdxFileStreamImplS *impl;
    cdx_int32 ret;
    cdx_int64 nHadReadLen;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    //* we must limit the HadReadLen within impl->size,
    //* or in some case will be wrong, such as cts
    nHadReadLen = ftell64(impl->fd) - impl->offset;
    if(nHadReadLen >= impl->size)
    {
        CDX_LOGD("eos, pos(%lld)",impl->size);
        return 0;
    }

    if (WaitIdleAndSetState(&impl->state, FILE_STREAM_READING) != CDX_SUCCESS)
    {
        CDX_LOGE("set state(%d) fail.", CdxAtomicRead(&impl->state));
        return -1;
    }

    if((nHadReadLen + len) > impl->size)
    {
        len = impl->size - nHadReadLen;
    }

    ret = fread64(impl->fd, buf, len);

    if (ret < (cdx_int32)len)
    {
        if ((ftell64(impl->fd) - impl->offset) == impl->size) /*end of file*/
        {
            CDX_LOGD("eos, ret(%d), pos(%lld)...", ret, impl->size);
            impl->ioErr = CDX_IO_STATE_EOS;
        }
        else
        {
            impl->ioErr = errno;
            CDX_LOGE("ret(%d), errno(%d), cur pos:(%lld), impl->size(%lld)",
                    ret, impl->ioErr, ftell64(impl->fd) - impl->offset, impl->size);
        }
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    return ret;
}

static cdx_int32 __FileStreamClose(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int32 ret;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    ret = WaitIdleAndSetState(&impl->state, FILE_STREAM_CLOSING);

    CDX_FORCE_CHECK(CDX_SUCCESS == ret);

    //* the fd may be invalid when close, such as in TF-card test
    ret = fclose64(impl->fd);
    if(ret != 0)
    {
        logw(" close fd may be not normal, ret = %d, errno = %d",ret,errno);
    }

    if (impl->probeData.buf)
    {
        CdxFree(impl->probeData.buf);
    }
    if (impl->filePath)
    {
        CdxFree(impl->filePath);
    }
    if(impl->redriectPath)
    {
        CdxFree(impl->redriectPath);
    }
    CdxFree(impl);
    // TODO: use refence
    return CDX_SUCCESS;
}

static cdx_int32 __FileStreamGetIoState(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    return impl->ioErr;
}

static cdx_uint32 __FileStreamAttribute(CdxStreamT *stream)
{
    CDX_UNUSE(stream);
    return CDX_STREAM_FLAG_SEEK;
}

static cdx_int32 __FileStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{

    struct CdxFileStreamImplS *impl;

    CDX_UNUSE(param);

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    switch (cmd)
    {
    default :
        break;
    }

    return CDX_SUCCESS;
}

static cdx_int32 __FileStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    struct CdxFileStreamImplS *impl;
    cdx_int64 ret;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (WaitIdleAndSetState(&impl->state, FILE_STREAM_SEEKING) != CDX_SUCCESS)
    {
        CDX_LOGE("set state(%d) fail.", CdxAtomicRead(&impl->state));
        impl->ioErr = CDX_IO_STATE_INVALID;
        return -1;
    }

    switch (whence)
    {
    case STREAM_SEEK_SET:
    {
        if (offset < 0 || offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)", offset, impl->size);
            CdxDumpThreadStack(gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        ret = fseek64(impl->fd, impl->offset + offset, SEEK_SET);
        break;
    }
    case STREAM_SEEK_CUR:
    {
        cdx_int64 curPos = ftell64(impl->fd) - impl->offset;
        if (curPos + offset < 0 || curPos + offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld), curPos(%lld)",
                      offset, impl->size, curPos);
            CdxDumpThreadStack(gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        ret = fseek64(impl->fd, offset, SEEK_CUR);
        break;
    }
    case STREAM_SEEK_END:
    {
        cdx_int64 absOffset = impl->offset + impl->size + offset;
        if (absOffset < impl->offset || absOffset > impl->offset + impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)",
                      absOffset, impl->offset + impl->size);
            CdxDumpThreadStack(gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        ret = fseek64(impl->fd, absOffset, SEEK_SET);
        break;
    }
    default :
        CDX_CHECK(0);
        break;
    }

    if (ret < 0)
    {
        impl->ioErr = errno;
        CDX_LOGE("seek failure, io error(%d); 'whence(%d), base-offset(%lld), offset(%lld)' ",
                impl->ioErr, whence, impl->offset, offset);
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    return (ret >= 0 ? 0 : -1);
}

static cdx_int64 __FileStreamTell(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int64 pos;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);
    pos = ftell64(impl->fd) - impl->offset;
    if (-1 == pos)
    {
        impl->ioErr = errno;
        CDX_LOGE("ftello failure, io error(%d)", impl->ioErr);
    }
    return pos;
}

static cdx_bool __FileStreamEos(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int64 pos = -1;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);
    pos = ftell64(impl->fd) - impl->offset;
    CDX_LOGD("(%lld / %lld / %lld)", pos, impl->offset, impl->size);
    return (pos == impl->size);
}

static cdx_int64 __FileStreamSize(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    return impl->size;
}

static cdx_int32 __FileStreamGetMetaData(CdxStreamT *stream, const cdx_char *key, void **pVal)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (strcmp(key, "uri") == 0)
    {
        *pVal = impl->filePath;
        return 0;
    }
    else if (strcmp(key, STREAM_METADATA_REDIRECT_URI) == 0)
    {
        CDX_LOGD("redriect url '%s'", impl->redriectPath);
        *pVal = impl->redriectPath;
        return 0;
    }

    CDX_LOGW("key(%s) not found...", key);
    return -1;
}

cdx_int32 __StreamConnect(CdxStreamT *stream)
{
    cdx_int32 ret = 0;
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (strncmp(impl->filePath, FILE_STREAM_SCHEME, 7) == 0) /*file://... */
    {
        impl->fd = fopen64(impl->filePath + 7);
        if (impl->fd <= 0)
        {
            CDX_LOGE("open file failure, errno(%d)", errno);
            ret = -1;
            goto failure;
        }

        impl->offset = 0;
        impl->size = fseek64(impl->fd, 0, SEEK_END);
        ret = (cdx_int32)fseek64(impl->fd, 0, SEEK_SET);
        CDX_LOG_CHECK(ret == 0, "errno(%d)", errno);
    }
    else if (strncmp(impl->filePath, FD_STREAM_SCHEME, 5) == 0) /*fd://... */
    {
        int tmpFd;
        ret = sscanf(impl->filePath, "fd://%d?offset=%lld&length=%lld",
                     &tmpFd, &impl->offset, &impl->size);
        if (ret != 3)
        {
            CDX_LOGE("sscanf failure...(%s)", impl->filePath);
            ret = -1;
            goto failure;
        }

        if (tmpFd <= 0)
        {
            CDX_LOGE("invalid fd(%d)", tmpFd);
            ret = -1;
            goto failure;
        }

        impl->fd = dup(tmpFd);
        if (impl->fd <= 0)
        {
            CDX_LOGE("dup fd failure, errno(%d)", errno);
            ret = -1;
            goto failure;
        }

        if (impl->offset < 0)
        {
            CDX_LOGW("invalid offset(%lld)", impl->offset);
            impl->offset = 0;
        }

        if (impl->size <= 0)
        {
            cdx_int64 size;
            CDX_LOGW("invalid size(%lld), try to get it myself...", impl->size);

            size = fseek64(impl->fd, 0, SEEK_END);
            impl->size = size - impl->offset;

            CDX_LOGW("got it, size(%lld)", impl->size);
        }

        ret = fseek64(impl->fd, impl->offset, SEEK_SET);
        if (ret < 0)
        {
            CDX_LOGE("seek to offset(%lld) failure, errno(%d)", impl->offset, errno);
            ret = -1;
            goto failure;
        }
        impl->redriectPath = FdToFilepath(impl->fd);
        CDX_LOGD("(%d/%lld/%lld) path:'%s'",
                  impl->fd, impl->offset, impl->size, impl->redriectPath);
    }
    else
    {
        CDX_LOG_CHECK(0, "uri(%s) not file stream...", impl->filePath);
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    impl->probeData.buf = CdxMalloc(DEFAULT_PROBE_DATA_LEN);
    impl->probeData.len = DEFAULT_PROBE_DATA_LEN;
    impl->ioErr = CDX_SUCCESS;

    /* if data not enough, only probe 'size' data */
    if (impl->size > 0 && impl->size < DEFAULT_PROBE_DATA_LEN)
    {
        CDX_LOGW("File too small, size(%lld), will read all for probe...", impl->size);
        impl->probeData.len = impl->size;
    }
    ret = fread64(impl->fd, impl->probeData.buf, impl->probeData.len);
    if (ret < (int)impl->probeData.len)
    {
        CDX_LOGW("io fail, errno=%d", errno);
        ret = -1;
        goto failure;
    }

    CDX_BUF_DUMP(impl->probeData.buf, 16);

    ret = (cdx_int32)fseek64(impl->fd, impl->offset, SEEK_SET);
    if (-1 == ret)
    {
        CDX_LOGW("io fail errno(%d)", errno);
        ret = -1;
        goto failure;
    }

    impl->ioErr = 0;
    return ret;

failure:
    return ret;

}
#endif

#if 1

static int lock(struct CdxVideoResizeStreamImplS *impl)
{
    //logd("*********impl->mutex=%x\n", impl->mutex==0);
    if(pthread_mutex_lock(&impl->mutex) != 0)
        return -1;
    return 0;
}

static void unlock(struct CdxVideoResizeStreamImplS *impl)
{
    pthread_mutex_unlock(&impl->mutex);
    return;
}

static inline cdx_int32 WaitIdleAndSetState(cdx_atomic_t *state, cdx_ssize val)
{
    while (!CdxAtomicCAS(state, FILE_STREAM_IDLE, val))
    {
        if (CdxAtomicRead(state) == FILE_STREAM_CLOSING)
        {
            CDX_LOGW("file is closing.");
            return CDX_FAILURE;
        }
    }
    return CDX_SUCCESS;
}

static CdxStreamProbeDataT *__VideoResizeStreamGetProbeData(CdxStreamT *stream)
{
    struct CdxVideoResizeStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    return &impl->probeData;
}

static cdx_uint32 __VideoResizeStreamAttribute(CdxStreamT *stream)
{
    CDX_UNUSE(stream);
    return CDX_STREAM_FLAG_SEEK;
}

static cdx_int32 __VideoResizeStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    struct CdxVideoResizeStreamImplS *impl;
    cdx_int64 ret;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    if (WaitIdleAndSetState(&impl->state, FILE_STREAM_SEEKING) != CDX_SUCCESS)
    {
        CDX_LOGE("set state(%d) fail.", CdxAtomicRead(&impl->state));
        impl->ioErr = CDX_IO_STATE_INVALID;
        return -1;
    }

    switch (whence)
    {
    case STREAM_SEEK_SET:
    {
#if 0
        if (offset < 0 || offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)", offset, impl->size);
            CdxDumpThreadStack(gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        impl->offset = offset;
#endif
        ret = 0;
        break;
    }
    case STREAM_SEEK_CUR:
    {
#if 0
        cdx_int64 curPos = impl->offset;
        if (curPos + offset < 0 || curPos + offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld), curPos(%lld)",
                      offset, impl->size, curPos);
            CdxDumpThreadStack(gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        impl->offset += offset;
#endif
        ret = 0;
//        ret = fseek64(impl->fd, offset, SEEK_CUR);
        break;
    }
    case STREAM_SEEK_END:
    {
#if 0
        cdx_int64 absOffset = (offset >= 0) ? offset : (0 - offset);
        if (absOffset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)",
                      absOffset, impl->offset + impl->size);
            CdxDumpThreadStack(gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        impl->offset = impl->size - absOffset;
#endif
        ret = 0;
        break;
    }
    default :
        CDX_CHECK(0);
        break;
    }

    if (ret < 0)
    {
        impl->ioErr = errno;
        CDX_LOGE("seek failure, io error(%d); 'whence(%d), offset(%lld)' ",
                impl->ioErr, whence, offset);
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    return (ret >= 0 ? 0 : -1);
}

static cdx_int64 __VideoResizeStreamTell(CdxStreamT *stream)
{
    struct CdxVideoResizeStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);
    return 0;
}

static cdx_bool __VideoResizeStreamEos(CdxStreamT *stream)
{
    struct CdxVideoResizeStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);
   // CDX_LOGD("(%lld / %lld / %lld)", pos, impl->offset, impl->size);
    return (impl->ioErr == CDX_IO_STATE_EOS)? 1 : 0;

}

static cdx_int64 __VideoResizeStreamSize(CdxStreamT *stream)
{
    struct CdxVideoResizeStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    //return impl->size;
    return 0;
}

static cdx_int32 __VideoResizeStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    struct CdxVideoResizeStreamImplS *impl;
    cdx_int32 len1 = 0;
    cdx_int32 len2 = 0;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    //* we must limit the HadReadLen within impl->size,
    //* or in some case will be wrong, such as cts

    if (WaitIdleAndSetState(&impl->state, FILE_STREAM_READING) != CDX_SUCCESS)
    {
        CDX_LOGE("set state(%d) fail.", CdxAtomicRead(&impl->state));
        return -1;
    }
    while(impl->eosFlag != 1)
    {
        if(impl->validDataSize < len)
        {
#if 0
            if(impl->eosFlag==1)
            {
                len = impl->validDataSize;
                break;
            }
            else
#endif
            {
                usleep(10000);
                continue;
            }
        }
        break;
    }
    if(impl->eosFlag == 1)
    {
         impl->ioErr = CDX_IO_STATE_EOS;
         CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
         return 0;
    }


    if((impl->pReadPtr+len) >= impl->bufferEnd)
    {
        len1 = impl->bufferEnd-impl->pReadPtr;
        len2 = len - len1;
        memcpy((void*)buf, (void*)impl->pReadPtr, len1);
        memcpy((void*)((intptr_t)buf+len1), (void*)impl->buffer, len2);

        if(lock(impl) < 0)
        {
            CDX_LOGE("lock the impl error\n");
        }
        impl->pReadPtr = (unsigned char*)((intptr_t)impl->buffer+len2);
        impl->validDataSize -= len;
        impl->validBufSize += len;
        unlock(impl);
    }
    else
    {
        memcpy(buf, impl->pReadPtr, len);
        if(lock(impl) < 0)
        {
            CDX_LOGE("lock the impl error\n");
        }
        impl->pReadPtr += len;
        if(impl->pReadPtr == impl->bufferEnd)
        {
            impl->pReadPtr = (unsigned char*)impl->buffer;
        }
        impl->validDataSize -= len;
        impl->validBufSize += len;
        unlock(impl);
    }

    if((impl->eosFlag==1) && (impl->validDataSize==0))
    {
        CDX_LOGD("eos, ret, pos(%d)...",impl->validDataSize);
        impl->ioErr = CDX_IO_STATE_EOS;
    }
    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    return len;
}

static cdx_int32 __VideoResizeStreamClose(CdxStreamT *stream)
{
    struct CdxVideoResizeStreamImplS *impl;
    cdx_int32 ret;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    //logd("**************impl->state=%d\n", impl->state);

    ret = WaitIdleAndSetState(&impl->state, FILE_STREAM_CLOSING);

    CDX_FORCE_CHECK(CDX_SUCCESS == ret);

    //* the fd may be invalid when close, such as in TF-card tes
    if (impl->probeData.buf)
    {
        CdxFree(impl->probeData.buf);
    }
    if(impl->filePath)
    {
        CdxFree(impl->filePath);
    }
    if(impl->buffer)
    {
        CdxFree(impl->buffer);
    }
    pthread_mutex_destroy(&impl->mutex);
    CdxFree(impl);
    // TODO: use refence
    return CDX_SUCCESS;
}

static cdx_int32 __VideoResizeStreamGetIoState(CdxStreamT *stream)
{
    struct CdxVideoResizeStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    return impl->ioErr;
}

//static cdx_uint32 __VideoResizeStreamAttribute(CdxStreamT *stream)
//{
//    CDX_UNUSE(stream);
//    return CDX_STREAM_FLAG_SEEK;
//}

static cdx_int32 __VideoResizeStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{

    struct CdxVideoResizeStreamImplS *impl;
    StreamBufferInfo* bufInfo = NULL;

    CDX_UNUSE(param);

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);

    switch (cmd)
    {
        case STREAM_CMD_REQUEST_WRITE_INFO:
            if(impl->ioErr == CDX_IO_STATE_EOS)
            {
                bufInfo->bufPtr = NULL;
                bufInfo->bufLen  = 0;
            }
            else
            {
                bufInfo = (StreamBufferInfo*)param;
                bufInfo->bufPtr = impl->pWritePtr;
                bufInfo->bufLen = impl->validBufSize;
                if((intptr_t)(impl->pWritePtr+bufInfo->bufLen)>= (intptr_t)impl->bufferEnd)
                {
                    bufInfo->bufLen = impl->bufferEnd-bufInfo->bufPtr;
                }
            }
            break;
        case STREAM_CMD_UPDATE_WRITE_INFO:
            if(impl->ioErr == CDX_IO_STATE_EOS)
            {
                break;
            }
            else
            {
                if(lock(impl) < 0)
                {
                    return CDX_FAILURE;
                }
                bufInfo = (StreamBufferInfo*)param;

                impl->pWritePtr +=  bufInfo->bufLen;
                if(impl->pWritePtr == impl->bufferEnd)
                {
                    impl->pWritePtr = (unsigned char*)impl->buffer;
                }
                impl->validBufSize -=  bufInfo->bufLen;
                impl->validDataSize += bufInfo->bufLen;
                unlock(impl);
                break;
            }
        case STREAM_CMD_SET_FORCESTOP:
        case STREAM_CMD_SET_EOF:
            impl->eosFlag = 1;
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            break;
        default :
            break;
    }

    return CDX_SUCCESS;
}

cdx_int32 __VideoResizeStreamConnect(CdxStreamT *stream)
{
    cdx_int32 ret = 0;
    struct CdxVideoResizeStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxVideoResizeStreamImplS, base);


    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);

    while(impl->eosFlag == 0)
    {
        if(impl->validDataSize < DEFAULT_PROBE_DATA_LEN)
        {
            usleep(10000);
            continue;
        }
        break;
    }

    if(impl->eosFlag == 1)
    {
        return -1;
    }
    impl->probeData.buf = malloc(DEFAULT_PROBE_DATA_LEN);
    impl->probeData.len = DEFAULT_PROBE_DATA_LEN;
    impl->ioErr = CDX_SUCCESS;

    /* if data not enough, only probe 'size' data */
    if(lock(impl) < 0)
    {
        CDX_LOGE("lock the impl faile\n");
    }
    if (impl->validDataSize > 0 && impl->validDataSize < DEFAULT_PROBE_DATA_LEN)
    {
        CDX_LOGW("File too small, size(%d), will read all for probe...", impl->validDataSize);
        impl->probeData.len = impl->validDataSize;
    }
    unlock(impl);
    memcpy(impl->probeData.buf, impl->buffer, impl->probeData.len);
    CDX_BUF_DUMP(impl->probeData.buf, 16);
    impl->ioErr = 0;
    return ret;

//failure:
//    return ret;

}

#endif

static struct CdxStreamOpsS videoResizeStreamOps =
{
    .connect = __VideoResizeStreamConnect,
    .getProbeData = __VideoResizeStreamGetProbeData,
    .read = __VideoResizeStreamRead,
    .write = NULL,
    .close = __VideoResizeStreamClose,
    .getIOState = __VideoResizeStreamGetIoState,
    .attribute = __VideoResizeStreamAttribute,
    .control = __VideoResizeStreamControl,
    .getMetaData = NULL,
    .seek = __VideoResizeStreamSeek,
    .seekToTime = NULL,
    .eos = __VideoResizeStreamEos,
    .tell = __VideoResizeStreamTell,
    .size = __VideoResizeStreamSize,
};

static CdxStreamT *__VideoResizeStreamCreate(CdxDataSourceT *source)
{
    struct CdxVideoResizeStreamImplS *impl;
    cdx_char* bufAddr = 0;
    cdx_int32 len = 0;
    cdx_int32 ret = 0;
    cdx_char buf1[128];

    impl = CdxMalloc(sizeof(*impl));
    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(*impl));

    impl->base.ops = &videoResizeStreamOps;
    impl->filePath = CdxStrdup(source->uri);
    impl->ioErr = -1;

    sscanf(impl->filePath,"%128[^=]=%d", buf1, &len);
    bufAddr = malloc(len);
    if(bufAddr == NULL)
    {
        CDX_LOGE("malloc the buffer for bufAddr is failed\n");
    }
    impl->buffer = (cdx_char*)bufAddr;
    impl->pReadPtr = (unsigned char*)bufAddr;
    impl->pWritePtr = (unsigned char*)bufAddr;
    impl->validDataSize = 0;
    impl->validBufSize = len;
    impl->bufferSize = len;
    impl->bufferEnd = (unsigned char*)(bufAddr+len);
    impl->eosFlag   = 0;
    ret = pthread_mutex_init(&impl->mutex, NULL);
    if(ret != 0)
    {
        loge("***************create the mutex for stream is error\n");
    }
    return &impl->base;
}

CdxStreamCreatorT videoResizeStreamCtor =
{
    .create = __VideoResizeStreamCreate
};
