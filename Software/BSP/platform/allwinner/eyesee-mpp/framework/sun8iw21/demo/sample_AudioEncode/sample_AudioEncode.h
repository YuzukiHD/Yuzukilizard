#ifndef _SAMPLE_AUDIOENCODE_H_
#define _SAMPLE_AUDIOENCODE_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>

#include <Errors.h>

typedef struct SampleAudioEncodeCmdLineParam
{
    std::string mConfigFilePath;
}SampleAudioEncodeCmdLineParam;

typedef struct SampleAudioEncodeConfig
{
    int mPcmSampleRate;
    int mPcmChannelCnt;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mEncodeBitrate;
    std::string mFileDir;
    int mSegmentCount;
    int mSegmentDuration;   //unit:s.
}SampleAudioEncodeConfig;

class SampleAudioEncodeContext;

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleAudioEncodeContext *pOwner);
private:
    SampleAudioEncodeContext *const mpOwner;
};

class SampleAudioEncodeContext
{
public:
    SampleAudioEncodeContext();
    ~SampleAudioEncodeContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    std::string MakeSegmentFileName();
    SampleAudioEncodeCmdLineParam mCmdLinePara;
    SampleAudioEncodeConfig mConfigPara;
    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeRecorderListener mRecorderListener;

    int mMuxerId;
    MEDIA_FILE_FORMAT_E mFileFormat;
    std::deque<std::string> mSegmentFiles;
    int mFileNum;
};

#endif  /* _SAMPLE_AUDIOENCODE_H_ */

