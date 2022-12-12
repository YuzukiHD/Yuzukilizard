#define LOG_NDEBUG 0
#define LOG_TAG "dup2SeldomUsedFd"
#include <utils/plat_log.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include <dup2SeldomUsedFd.h>

#define FD_MIN (500)
#define FD_MAX (1000)
static int nCurUsedFd = -1;
static pthread_mutex_t gFdLock;

#define DUP2_MODE
//static void dup2SeldomUsedFdInit(void) __attribute__((constructor));
__attribute__((constructor)) static void dup2SeldomUsedFdInit(void)
{
    alogv("gFdLock init");
    int err = pthread_mutex_init(&gFdLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
}

__attribute__((destructor)) static void dup2SeldomUsedFdExit(void)
{
    alogv("gFdLock destroy");
    pthread_mutex_destroy(&gFdLock);
}

static int fd_is_valid(int fd)
{    
    //return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
    return fcntl(fd, F_GETFD) != -1;
}

int dup2SeldomUsedFd(int nSrcFd)
{
    ERRORTYPE ret = SUCCESS;
#ifdef DUP2_MODE
    int nDstFd;
    pthread_mutex_lock(&gFdLock);
    if(-1 == nCurUsedFd)
    {
        nDstFd = FD_MIN;
    }
    else
    {
        nDstFd = nCurUsedFd + 1;
        if(nDstFd >= FD_MAX)
        {
            alogd("fdNum will return head.");
            nDstFd = FD_MIN;
        }
    }
    int bSelect = 0;
    for(int i=0; i<FD_MAX-FD_MIN; i++)
    {
        if(fd_is_valid(nDstFd))
        {
            alogd("dstFd[%d] is valid? change next", nDstFd);
            nDstFd++;
            if(nDstFd >= FD_MAX)
            {
                nDstFd = FD_MIN;
            }
        }
        else
        {
            bSelect = 1;
            if(nSrcFd == nDstFd)
            {
                aloge("fatal error! why dstFd[%d] == srcFd[%d]?", nDstFd, nSrcFd);
            }
            else
            {
                alogv("select dstFd[%d] to dup2", nDstFd);
            }
            break;
        }
    }
    
    if(bSelect)
    {
        int dup2Ret = dup2(nSrcFd, nDstFd);
        if(dup2Ret == nDstFd)
        {
            alogv("dup2 srcFd[%d]->dstFd[%d] success", nSrcFd, nDstFd);
            nCurUsedFd = nDstFd;
            ret = SUCCESS;
        }
        else
        {
            aloge("fatal error! dup2 srcFd[%d]->dstFd[%d] fail[%d]!", nSrcFd, nDstFd, dup2Ret);
            ret = ERR_COMMON_UNEXIST;
        }
    }
    else
    {
        aloge("fatal error! all fds [%d, %d) are used?", FD_MIN, FD_MAX);
        ret = ERR_COMMON_SYS_NOTREADY;
    }
    
    pthread_mutex_unlock(&gFdLock);
    if(SUCCESS == ret)
    {
        return nDstFd;
    }
    else
    {
        return -1;
    }
#else
    int nDstFd = dup(nSrcFd);
    if(nDstFd >= 0)
    {
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! dup fd[%d] fail!", nSrcFd);
        ret = ERR_COMMON_UNEXIST;
    }
    if(SUCCESS == ret)
    {
        return nDstFd;
    }
    else
    {
        return -1;
    }
#endif
}

