
#ifndef _SAMPLE_VIDEO_RESIZER_H_
#define _SAMPLE_VIDEO_RESIZER_H_

#include <string>
#include <mm_comm_sys.h>
#include <tsemaphore.h>
#include <EyeseeVideoResizer.h>

typedef struct SamplePlayerCmdLineParam
{
    std::string mConfigFilePath;
}SamplePlayerCmdLineParam;

typedef struct SamplePlayerConfig
{
    std::string mSrcFilePath;
    // Output file param
    std::string mDstFilePath;
    std::string mDstBitStreamPath;
    int mDstVideoWidth;
    int mDstVideoHeight;
    int mDstFrameRate;
    int mDstBitRate;
    PAYLOAD_TYPE_E mDstVideoEncFmt;
    MEDIA_FILE_FORMAT_E mMediaFileFmt;
}SamplePlayerConfig;

class SamplePlayerContext;

class SamplePlayerContext
{
public:
    SamplePlayerContext();
    ~SamplePlayerContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    SamplePlayerCmdLineParam mCmdLinePara;
    SamplePlayerConfig mConfigPara;

    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;

    EyeseeLinux::EyeseeVideoResizer *mpVideoResizer;
};

#endif  /* _SAMPLE_VIDEO_RESIZER_H_ */

