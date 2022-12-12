
#ifndef _SAMPLE_THUMBRETRIEVER_H_
#define _SAMPLE_THUMBRETRIEVER_H_

#include <EyeseeThumbRetriever.h>

typedef struct SampleThumbCmdLineParam
{    
    std::string mConfigFilePath;
}SampleThumbCmdLineParam;


typedef struct SampleThumbConfig
{
    std::string mFilePath;
        
    int mJpegWidth;
    int mJpegHeight;
    int mSeekStartPosition;
    std::string mJpegPath;
    
}SampleThumbConfig;

class SampleThumbContext
{
public:
    SampleThumbContext();
    ~SampleThumbContext();

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);   
    EyeseeLinux::status_t loadConfig();    
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);   
    
    MPP_SYS_CONF_S mSysConf;
    SampleThumbCmdLineParam mCmdLinePara;
    SampleThumbConfig mConfigPara;
    EyeseeLinux::EyeseeThumbRetriever *mThumbRetriever;
};




#endif
