
//#define LOG_NDEBUG 0
#define LOG_TAG "FsWriter"
#include <utils/plat_log.h>

#include <string.h>
#include <stdint.h>

#include <SystemBase.h>
#include <FsWriter.h>
//#include <ConfigOption.h>

extern FsWriter *initFsDirectWrite(struct cdx_stream_info *pStream);
extern int deinitFsDirectWrite(FsWriter *pFsWriter);
extern FsWriter *initFsSimpleCache(struct cdx_stream_info *pStream, int nCacheSize);
extern int deinitFsSimpleCache(FsWriter *pFsWriter);
extern FsWriter *initFsCacheThreadContext(struct cdx_stream_info *pStream, char *pCache, int nCacheSize, unsigned int vCodec);
extern int deinitFsCacheThreadContext(FsWriter *pFsWriter);

FsWriter* createFsWriter(FSWRITEMODE mode, struct cdx_stream_info *pStream, char *pCache, unsigned int nCacheSize, unsigned int vCodec)
{
    if(FSWRITEMODE_CACHETHREAD == mode)
    {
        return initFsCacheThreadContext(pStream, pCache, nCacheSize, vCodec);
    }
    else if (FSWRITEMODE_SIMPLECACHE == mode)
    {
        return initFsSimpleCache(pStream, nCacheSize);
    }
    else if (FSWRITEMODE_DIRECT == mode)
    {
        return initFsDirectWrite(pStream);
    }
    else
    {
        aloge("not support mode[%d]", mode);
        return NULL;
    }
}

int destroyFsWriter(FsWriter *thiz)
{
    if (NULL == thiz) {
        aloge("FsWriter is NULL");
        return -1;
    }
    if(FSWRITEMODE_CACHETHREAD == thiz->mMode)
    {
        return deinitFsCacheThreadContext(thiz);
    }
    else if (FSWRITEMODE_SIMPLECACHE == thiz->mMode)
    {
        return deinitFsSimpleCache(thiz);
    }
    else if (FSWRITEMODE_DIRECT == thiz->mMode)
    {
        return deinitFsDirectWrite(thiz);
    }
    else
    {
        aloge("not support mode[%d]", thiz->mMode);
        return -1;
    }
    return 0;
}


#define SHOW_FWRITE_TIME
#define SHOW_TIME_THRESHOLD     (1000*1000ll) 

#define GLOBAL_STAT_FWRITE (0)
#if GLOBAL_STAT_FWRITE
static const int64_t gnFileWriteStatBytes = (int64_t)4*1024*1024*1024;
static int64_t gnTotalWritten = 0;  //unit:byte
static int64_t gnTotalTimeDuration = 0; //unit:us
static pthread_mutex_t gStatLock;
static void __attribute__((constructor)) FileWriteStatInit(void)
{
    alogw("FileWriteStatInit before main?");
    if(0!=pthread_mutex_init(&gStatLock, NULL))
    {
        aloge("fatal error! pthread_mutex init fail");
    }
    pthread_mutex_lock(&gStatLock);
    gnTotalWritten = 0;
    gnTotalTimeDuration = 0;
    pthread_mutex_unlock(&gStatLock);
};
static void __attribute__((destructor)) FileWriteStatExit(void)
{
    alogw("FileWriteStatExit after main?");
    if(0!=pthread_mutex_destroy(&gStatLock))
    {
        aloge("fatal error! pthread_mutex init fail");
    }
    alogd("stat total write [%lld]bytes, spend [%lld]us", gnTotalWritten, gnTotalTimeDuration);
};
#endif

ssize_t fileWriter(struct cdx_stream_info *pStream, const char *buffer, size_t size)
{
    ssize_t totalWriten = 0;
#ifdef SHOW_FWRITE_TIME
    int64_t tm1, tm2;
#endif

    if (pStream->writeError == EIO) 
    {
        return -EIO;
    }

#ifdef SHOW_FWRITE_TIME
    tm1 = CDX_GetSysTimeUsMonotonic();
#endif
    totalWriten = pStream->write(buffer, 1, size, pStream);
#ifdef SHOW_FWRITE_TIME
    tm2 = CDX_GetSysTimeUsMonotonic();
    if (tm2-tm1 > SHOW_TIME_THRESHOLD) 
    {
        alogw("write %d(req is %d) Bytes, [%lld]ms", totalWriten, size, (tm2-tm1)/1000);
    }
#endif
    if((size_t)(totalWriten) != size)
    {
        aloge("Stream[%p]fwrite error [%d]!=[%u](%s)", pStream, totalWriten, size, strerror(errno));
        if (errno == EIO) 
        {
            aloge("disk io error, stop write disk!!");
        }
        pStream->writeError = errno;
        if (pStream->writeErrcnt++ > 10 || pStream->writeError == EIO) 
        {
            if (pStream->callback.cb != NULL && pStream->callback.hComp != NULL) 
            {
                pStream->callback.cb(pStream->callback.hComp, 0);
            }
        }
    } 
    else 
    {
        if (pStream->writeErrcnt != 0) 
        {
            pStream->writeErrcnt = 0;
        }
    }
#if GLOBAL_STAT_FWRITE
    pthread_mutex_lock(&gStatLock);
    gnTotalWritten += totalWriten;
    gnTotalTimeDuration += (tm2-tm1);
    if(gnTotalWritten >= gnFileWriteStatBytes)
    {
        alogw("file write stat: write:[%lld]bytes, time:[%lld]us", gnTotalWritten, gnTotalTimeDuration);
        gnTotalWritten = 0;
        gnTotalTimeDuration = 0;
    }
    pthread_mutex_unlock(&gStatLock);
#endif
    return totalWriten;
}

