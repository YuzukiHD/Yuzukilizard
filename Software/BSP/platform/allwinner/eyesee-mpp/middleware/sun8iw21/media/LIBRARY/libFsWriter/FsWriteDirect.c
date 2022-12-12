
//#define LOG_NDEBUG 0
#define LOG_TAG "FsWriteDirect"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <FsWriter.h>
//#include <ConfigOption.h>

typedef struct tag_FsDirectContext
{
//    FILE        *mpFile;
//    int         mFd;
    struct cdx_stream_info *mpStream;
} FsDirectContext;

static ssize_t FsDirectWrite(FsWriter *thiz, const char *buf, size_t size)
{
    FsDirectContext *pCtx = (FsDirectContext*)thiz->mPriv;
    return fileWriter(pCtx->mpStream, buf, size);
}

static int FsDirectSeek(FsWriter *thiz, int64_t nOffset, int fromWhere)
{
    FsDirectContext *pCtx = (FsDirectContext*)thiz->mPriv;
    int ret;
    thiz->fsFlush(thiz);
    ret = pCtx->mpStream->seek(pCtx->mpStream, nOffset, fromWhere);
    return ret;
}

static int64_t FsDirectTell(FsWriter *thiz)
{
    FsDirectContext *pCtx = (FsDirectContext*)thiz->mPriv;
    int64_t nFilePos;
    thiz->fsFlush(thiz);
    nFilePos = pCtx->mpStream->tell(pCtx->mpStream);
    return nFilePos;
}

static int FsDirectTruncate(FsWriter *thiz, int64_t nLength)
{
    int ret;
    FsDirectContext *pCtx = (FsDirectContext*)thiz->mPriv;
    thiz->fsFlush(thiz);
    ret = pCtx->mpStream->truncate(pCtx->mpStream, nLength);
    return ret;
}

static int FsDirectFlush(FsWriter *thiz)
{
	return 0;
}

FsWriter *initFsDirectWrite(struct cdx_stream_info *pStream)
{
    FsWriter *pFsWriter = (FsWriter*)malloc(sizeof(FsWriter));
	if (NULL == pFsWriter) {
        aloge("Failed to alloc FsWriter(%s)", strerror(errno));
		return NULL;
	}
    memset(pFsWriter, 0, sizeof(FsWriter));

    FsDirectContext *pContext = (FsDirectContext*)malloc(sizeof(FsDirectContext));
	if (NULL == pContext) {
        aloge("Failed to alloc FsDirectContext(%s)", strerror(errno));
        free(pFsWriter);
		return NULL;
	}
    memset(pContext, 0, sizeof(FsDirectContext));

    pContext->mpStream = pStream;
	pFsWriter->fsWrite = FsDirectWrite;
    pFsWriter->fsSeek = FsDirectSeek;
    pFsWriter->fsTell = FsDirectTell;
    pFsWriter->fsTruncate = FsDirectTruncate;
	pFsWriter->fsFlush = FsDirectFlush;
	pFsWriter->mMode = FSWRITEMODE_DIRECT;
    pFsWriter->mPriv = (void*)pContext;
    return pFsWriter;
}

int deinitFsDirectWrite(FsWriter *pFsWriter)
{
	if (NULL == pFsWriter) {
        aloge("pFsWriter is NULL!!");
		return -1;
	}
	FsDirectContext *pContext = (FsDirectContext*)pFsWriter->mPriv;
	free(pContext);
	free(pFsWriter);
	return 0;
}
