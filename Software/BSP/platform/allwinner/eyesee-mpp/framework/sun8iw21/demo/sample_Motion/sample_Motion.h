#ifndef _SAMPLE_MOTION_H_
#define _SAMPLE_MOTION_H_

#include <EyeseeMotion.h>

typedef struct SampleMotionCmdLineParam
{
    std::string mConfigFilePath;
}SampleMotionCmdLineParam;

typedef struct SampleMotionConfig
{
    std::string mFilePath;

    int mJpegWidth;
    int mJpegHeight;
    int mStartTimeUs;
    int mFrameNums;
    int mFrameStep;
    int mObjectsNums;

    std::string mJpegPath;
}SampleMotionConfig;

class SampleMotionContext
{
public:   
    SampleMotionContext();
    ~SampleMotionContext();

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);   
    EyeseeLinux::status_t loadConfig();    
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);  

    MPP_SYS_CONF_S mSysConf;
    SampleMotionCmdLineParam mCmdLineParam;
    SampleMotionConfig  mConfigParam;

    EyeseeLinux::EyeseeMotion   *mpMotion;
};

#endif
