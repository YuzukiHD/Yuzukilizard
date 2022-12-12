
#ifndef _SAMPLE_LARGEFILE_H_
#define _SAMPLE_LARGEFILE_H_

#include <string>
#include <tsemaphore.h>
#include <CMediaMemory.h>
#include <Errors.h>

typedef struct SampleLargeFileCmdLineParam
{
    std::string mConfigFilePath;
}SampleLargeFileCmdLineParam;

typedef struct SampleLargeFileConfig
{
    int64_t mChunkSize;
    int64_t mTotalSize;
    std::string mFileDir;
    std::string mFileName;
}SampleLargeFileConfig;

class SampleLargeFileContext
{
public:
    SampleLargeFileContext();
    ~SampleLargeFileContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    void CreateChunk();
    SampleLargeFileCmdLineParam mCmdLinePara;
    SampleLargeFileConfig mConfigPara;

    cdx_sem_t mSemExit;

    EyeseeLinux::CMediaMemory *mpChunkBuf;
    int64_t mCurWriteSize;
    int mFd;
};

#endif  /* _SAMPLE_LARGEFILE_H_ */

