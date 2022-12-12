#ifndef _SAMPLE_DRIVERVIPP_H_
#define _SAMPLE_DRIVERVIPP_H_

#include <stdbool.h>
#include <pthread.h>

#include <cedarx_stream.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleDirectIOReadCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleDirectIOReadCmdLineParam;

typedef struct SampleDirectIOReadConfig
{
    char mSrcFilePath[MAX_FILE_PATH_SIZE];
    char mDstFilePath[MAX_FILE_PATH_SIZE];
    int mReadSize;      //unit:bytes
    int mReadInterval;  //unit:ms
    int mLoopCnt;   //0:read repeatly forever
}SampleDirectIOReadConfig;

typedef struct SampleDirectIOReadContext
{
    SampleDirectIOReadCmdLineParam mCmdLinePara;
    SampleDirectIOReadConfig mConfigPara;

    int mExitFlag;
    char *mpBuf;
    cdx_stream_info_t *mReadStream;
    CedarXDataSourceDesc mDatasourceDescForRead;
    cdx_stream_info_t *mWriteStream;
    CedarXDataSourceDesc mDatasourceDescForWrite;
}SampleDirectIOReadContext;

SampleDirectIOReadContext* constructSampleDirectIOReadContext();
void destructSampleDirectIOReadContext(SampleDirectIOReadContext *pContext);

#endif  /* _SAMPLE_DRIVERVIPP_H_ */

