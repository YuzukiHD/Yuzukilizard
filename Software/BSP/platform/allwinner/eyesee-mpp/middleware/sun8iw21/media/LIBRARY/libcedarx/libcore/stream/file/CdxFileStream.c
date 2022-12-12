/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFileStream.c
 * Description : File Stream Definition
 * History :
 *
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
#include <CdxDebug.h>

#define FILE_STREAM_SCHEME "file://"
#define FD_STREAM_SCHEME "fd://"
#define DEFAULT_PROBE_DATA_LEN (1024 * 128)

#define cdxfopen64(uri) open(uri, O_LARGEFILE)

#define cdxfseek64(fd, offset, whence) lseek64(fd, offset, whence)

#define cdxftell64(fd) lseek64(fd, 0, SEEK_CUR)

#define cdxfread64(fd, buf, len) read(fd, buf, len)

//#define feof64(fd) lseek64(fd, 0, SEEK_CUR)

#define cdxfclose64(fd) close(fd)

enum FileStreamStateE
{
    FILE_STREAM_IDLE = 0x00L,
    FILE_STREAM_READING = 0x01L,
    FILE_STREAM_SEEKING = 0x02L,
    FILE_STREAM_CLOSING = 0x03L,
    FILE_STREAM_WRITING = 0x04L,
};

/*fmt: "file://xxx" */
struct CdxFileStreamImplS
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
    nHadReadLen = cdxftell64(impl->fd) - impl->offset;
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

    ret = cdxfread64(impl->fd, buf, len);

    if (ret < (cdx_int32)len)
    {
        if ((cdxftell64(impl->fd) - impl->offset) == impl->size) /*end of file*/
        {
            CDX_LOGD("eos, ret(%d), pos(%lld)...", ret, impl->size);
            impl->ioErr = CDX_IO_STATE_EOS;
        }
        else
        {
            impl->ioErr = errno;
            CDX_LOGE("ret(%d), errno(%d), cur pos:(%lld), impl->size(%lld)",
                    ret, impl->ioErr, cdxftell64(impl->fd) - impl->offset, impl->size);
        }
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    return ret;
}

static cdx_int32 __FileStreamClose(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int32 ret;

    logd("FileStreamClose");

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    ret = WaitIdleAndSetState(&impl->state, FILE_STREAM_CLOSING);

    CDX_FORCE_CHECK(CDX_SUCCESS == ret);

    //* the fd may be invalid when close, such as in TF-card test
    ret = cdxfclose64(impl->fd);
    if(ret != 0)
    {
        logw(" close fd may be not normal, ret = %d, errno = %d",ret,errno);
    }

    if (impl->probeData.buf)
    {
        CdxFree(impl->probeData.buf);
        impl->probeData.buf = NULL;
    }
    if (impl->filePath)
    {
        CdxFree(impl->filePath);
        impl->filePath = NULL;
    }
    if(impl->redriectPath)
    {
        CdxFree(impl->redriectPath);
        impl->redriectPath = NULL;
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
    cdx_int64 ret = 0;

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
            CdxDumpThreadStack((pthread_t)gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        ret = cdxfseek64(impl->fd, impl->offset + offset, SEEK_SET);
        break;
    }
    case STREAM_SEEK_CUR:
    {
        cdx_int64 curPos = cdxftell64(impl->fd) - impl->offset;
        if (curPos + offset < 0 || curPos + offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld), curPos(%lld)",
                     offset, impl->size, curPos);
            CdxDumpThreadStack((pthread_t)gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        ret = cdxfseek64(impl->fd, offset, SEEK_CUR);
        break;
    }
    case STREAM_SEEK_END:
    {
        cdx_int64 absOffset = impl->offset + impl->size + offset;
        if (absOffset < impl->offset || absOffset > impl->offset + impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)",
                     absOffset, impl->offset + impl->size);
            CdxDumpThreadStack((pthread_t)gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        ret = cdxfseek64(impl->fd, absOffset, SEEK_SET);
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
    pos = cdxftell64(impl->fd) - impl->offset;
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
    pos = cdxftell64(impl->fd) - impl->offset;
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

cdx_int32 __FileStreamConnect(CdxStreamT *stream)
{
    cdx_int32 ret = 0;
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (strncmp(impl->filePath, FILE_STREAM_SCHEME, 7) == 0) /*file://... */
    {
        impl->fd = cdxfopen64(impl->filePath + 7);
        if (impl->fd <= 0)
        {
            CDX_LOGE("open file failure, errno(%d)", errno);
            ret = -1;
            goto failure;
        }

        impl->offset = 0;
        impl->size = cdxfseek64(impl->fd, 0, SEEK_END);
        logd("    *************impl->size=%lld",     impl->size);
        ret = (cdx_int32)cdxfseek64(impl->fd, 0, SEEK_SET);
        CDX_LOG_CHECK(ret == 0, "errno(%d)", errno);
        if(impl->filePath)
        {
            free(impl->filePath);
            impl->filePath = NULL;
        }
        cdx_char  newPath[5120] = {0};
        cdx_int64 fileOffset = 0;
        ret = sprintf(newPath, "fd://%d?offset=%lld&length=%lld", impl->fd, fileOffset, impl->size);
        impl->filePath = strdup(newPath);
        logd("impl->filePath=%s", impl->filePath);

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

            size = cdxfseek64(impl->fd, 0, SEEK_END);
            impl->size = size - impl->offset;

            CDX_LOGW("got it, size(%lld)", impl->size);
        }

        ret = cdxfseek64(impl->fd, impl->offset, SEEK_SET);
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
    ret = cdxfread64(impl->fd, impl->probeData.buf, impl->probeData.len);
    if (ret < (int)impl->probeData.len)
    {
        CDX_LOGW("io fail, errno=%d", errno);
        ret = -1;
        goto failure;
    }

    CDX_BUF_DUMP(impl->probeData.buf, 16);

    ret = (cdx_int32)cdxfseek64(impl->fd, impl->offset, SEEK_SET);
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

static struct CdxStreamOpsS fileStreamOps =
{
    .connect = __FileStreamConnect,
    .getProbeData = __FileStreamGetProbeData,
    .read = __FileStreamRead,
    .write = NULL,
    .close = __FileStreamClose,
    .getIOState = __FileStreamGetIoState,
    .attribute = __FileStreamAttribute,
    .control = __FileStreamControl,
    .getMetaData = __FileStreamGetMetaData,
    .seek = __FileStreamSeek,
    .seekToTime = NULL,
    .eos = __FileStreamEos,
    .tell = __FileStreamTell,
    .size = __FileStreamSize,
};

static CdxStreamT *__FileStreamCreate(CdxDataSourceT *source)
{
    struct CdxFileStreamImplS *impl;

    impl = CdxMalloc(sizeof(*impl));
    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(*impl));

    impl->base.ops = &fileStreamOps;
    impl->filePath = CdxStrdup(source->uri);
    impl->ioErr = -1;
    CDX_LOGD("local file '%s'", source->uri);

    return &impl->base;
}

CdxStreamCreatorT fileStreamCtor =
{
    .create = __FileStreamCreate
};

