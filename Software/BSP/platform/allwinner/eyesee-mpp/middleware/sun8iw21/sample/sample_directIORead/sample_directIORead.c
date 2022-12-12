
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

#include <plat_type.h>
#include <plat_log.h>
#include <confparser.h>

#include "sample_directIORead_conf.h"
#include "sample_directIORead.h"

SampleDirectIOReadContext *gpSampleDirectIOReadContext = NULL;

SampleDirectIOReadContext* constructSampleDirectIOReadContext()
{
    SampleDirectIOReadContext *pContext = (SampleDirectIOReadContext*)malloc(sizeof(SampleDirectIOReadContext));
    memset(pContext, 0, sizeof(SampleDirectIOReadContext));
    return pContext;
}

void destructSampleDirectIOReadContext(SampleDirectIOReadContext *pContext)
{
    if(pContext->mReadStream != NULL)
    {
        aloge("fatal error! why readStream is not free?");
    }
    if(pContext->mWriteStream != NULL)
    {
        aloge("fatal error! why writeStream is not free?");
    }
    if(pContext->mpBuf)
    {
        aloge("fatal error! why buf is not free?");
        free(pContext->mpBuf);
        pContext->mpBuf = NULL;
    }
    free(pContext);
}

static int loadSampleDirectIOReadConfig(SampleDirectIOReadContext *pContext, const char *pConfPath)
{
    if(NULL == pConfPath || 0 == strlen(pConfPath))
    {
        //use default value
        strcpy(pContext->mConfigPara.mSrcFilePath, "/mnt/extsd/1080p.mp4");
        strcpy(pContext->mConfigPara.mDstFilePath, "");
        pContext->mConfigPara.mReadSize = 1024;
        pContext->mConfigPara.mReadInterval = 0;
        pContext->mConfigPara.mLoopCnt = 1;
    }
    else
    {
        CONFPARSER_S stConfParser;
        int ret = createConfParser(pConfPath, &stConfParser);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        char *ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_DIRECTIOREAD_SRC_FILE, NULL);
        strcpy(pContext->mConfigPara.mSrcFilePath, ptr);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_DIRECTIOREAD_DST_FILE, NULL);
        if(ptr == NULL)
        {
            aloge("Be careful! dstFile is NULL?");
        }
        else
        {
            alogd("ptr[%p], size:%d", ptr, strlen(ptr));
            strcpy(pContext->mConfigPara.mDstFilePath, ptr);
        }
        pContext->mConfigPara.mReadSize = GetConfParaInt(&stConfParser, SAMPLE_DIRECTIOREAD_READ_SIZE, 0);
        pContext->mConfigPara.mReadInterval = GetConfParaInt(&stConfParser, SAMPLE_DIRECTIOREAD_READ_INTERVAL, 0);
        pContext->mConfigPara.mLoopCnt = GetConfParaInt(&stConfParser, SAMPLE_DIRECTIOREAD_LOOP_CNT, 0);
        destroyConfParser(&stConfParser);
        alogd("srcFile[%s], dstFile[%s],[%d-%d-%d]", pContext->mConfigPara.mSrcFilePath, pContext->mConfigPara.mDstFilePath,
            pContext->mConfigPara.mReadSize, pContext->mConfigPara.mReadInterval, pContext->mConfigPara.mLoopCnt);
    }
    return SUCCESS;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleDirectIOReadContext)
    {
        gpSampleDirectIOReadContext->mExitFlag++;
        alogd("set ExitFlag:%d", gpSampleDirectIOReadContext->mExitFlag);
    }
}

static ERRORTYPE parseCmdLine(SampleDirectIOReadContext *pContext, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

    if(argc <= 1)
    {
        alogd("use default config.");
        return SUCCESS;
    }
    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_SIZE)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_directIORead.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int result = 0;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);
    
    alogd("sample_directIORead running!");
    SampleDirectIOReadContext *pContext = constructSampleDirectIOReadContext();
    gpSampleDirectIOReadContext = pContext;
    parseCmdLine(pContext, argc, argv);
    if(loadSampleDirectIOReadConfig(pContext, pContext->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! load config fail, exit!");
        result = FAILURE;
        goto _exit0;
    }
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    pContext->mpBuf = (char*)malloc(pContext->mConfigPara.mReadSize);
    if(NULL == pContext->mpBuf)
    {
        aloge("fatal error! malloc %d bytes fail!", pContext->mConfigPara.mReadSize);
    }
    else
    {
        if(0 == ((int64_t)pContext->mpBuf%512))
        {
            alogd("pBuf[%p] can mod 512!", pContext->mpBuf);
        }
        else
        {
            alogd("pBuf[%p] can't mod 512!", pContext->mpBuf);
        }
    }
    int i = 0;
    while(0 == pContext->mExitFlag)
    {
        memset(&pContext->mDatasourceDescForRead, 0, sizeof(CedarXDataSourceDesc));
        pContext->mDatasourceDescForRead.stream_type=CEDARX_STREAM_LOCALFILE;
        pContext->mDatasourceDescForRead.source_type=CEDARX_SOURCE_FD;
        pContext->mDatasourceDescForRead.ext_fd_desc.fd = open(pContext->mConfigPara.mSrcFilePath, O_RDONLY);
        if(pContext->mDatasourceDescForRead.ext_fd_desc.fd < 0)
        {
            aloge("fatal error! why open [%s] fail?", pContext->mConfigPara.mSrcFilePath);
        }
        pContext->mReadStream = create_stream_handle(&pContext->mDatasourceDescForRead);
        close(pContext->mDatasourceDescForRead.ext_fd_desc.fd);
        pContext->mDatasourceDescForRead.ext_fd_desc.fd = -1;
        if(NULL == pContext->mReadStream)
        {
            aloge("fatal error! check code!");
        }

        if(strlen(pContext->mConfigPara.mDstFilePath) > 0)
        {
            //create writeStream to write data which is read from srcFile.
            memset(&pContext->mDatasourceDescForWrite, 0, sizeof(CedarXDataSourceDesc));
            pContext->mDatasourceDescForWrite.stream_type=CEDARX_STREAM_LOCALFILE;
            pContext->mDatasourceDescForWrite.source_type=CEDARX_SOURCE_FD;
            pContext->mDatasourceDescForWrite.ext_fd_desc.fd = open(pContext->mConfigPara.mDstFilePath, O_CREAT | O_TRUNC | O_RDWR);
            if(pContext->mDatasourceDescForWrite.ext_fd_desc.fd < 0)
            {
                aloge("fatal error! why open [%s] fail?", pContext->mConfigPara.mDstFilePath);
            }
            pContext->mWriteStream = create_outstream_handle(&pContext->mDatasourceDescForWrite);
            close(pContext->mDatasourceDescForWrite.ext_fd_desc.fd);
            pContext->mDatasourceDescForWrite.ext_fd_desc.fd = -1;
            if(NULL == pContext->mWriteStream)
            {
                aloge("fatal error! check code!");
            }
        }


        ssize_t nReadBytes = 0;
        ssize_t nWriteBytes = 0;
        while(1)
        {
            nReadBytes = pContext->mReadStream->read(pContext->mpBuf, 1, pContext->mConfigPara.mReadSize, pContext->mReadStream);
            if(nReadBytes > 0)
            {
                if(pContext->mWriteStream)
                {
                    nWriteBytes = pContext->mWriteStream->write(pContext->mpBuf, 1, nReadBytes, pContext->mWriteStream);
                    if(nWriteBytes != nReadBytes)
                    {
                        aloge("fatal error! why writeBytes[%d] != readBytes[%d]", nWriteBytes, nReadBytes);
                    }
                }
            }
            if(nReadBytes == pContext->mConfigPara.mReadSize)
            {
                if(pContext->mConfigPara.mReadInterval > 0)
                {
                    usleep(pContext->mConfigPara.mReadInterval*1000);
                }
            }
            else if(nReadBytes < pContext->mConfigPara.mReadSize)
            {
                if(nReadBytes < 0)
                {
                    aloge("fatal error! read [%d]?", nReadBytes);
                }
                else
                {
                    alogd("read over,[%d] < [%d]", nReadBytes, pContext->mConfigPara.mReadSize);
                }
                break;
            }
            else
            {
                aloge("fatal error! Impossible, check code![%d,%d]", nReadBytes, pContext->mConfigPara.mReadSize);
                break;
            }

            if(pContext->mExitFlag)
            {
                alogd("detect exit flag[%d], exit.", pContext->mExitFlag);
                break;
            }
        }

        destory_stream_handle(pContext->mReadStream);
        pContext->mReadStream = NULL;
        if(pContext->mWriteStream)
        {
            destroy_outstream_handle(pContext->mWriteStream);
            pContext->mWriteStream = NULL;
        }

        i++;
        if(pContext->mConfigPara.mLoopCnt > 0)
        {
            if(i >= pContext->mConfigPara.mLoopCnt)
            {
                alogd("finish [%d]tests", i);
                break;
            }
        }
        alogd("test[%d] done.", i);
    }
    if(pContext->mpBuf)
    {
        free(pContext->mpBuf);
        pContext->mpBuf = NULL;
    }
    destructSampleDirectIOReadContext(pContext);
    gpSampleDirectIOReadContext = pContext = NULL;
    alogd("sample_directIORead success exit, ret:%d!", result);
    return result;

_exit0:
    destructSampleDirectIOReadContext(pContext);
    gpSampleDirectIOReadContext = pContext = NULL;
    alogd("sample_directIORead exit, ret(%d)!\n", result);
    log_quit();
    return result;
}

