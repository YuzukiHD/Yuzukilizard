#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log/glog_helper.h"

//将信息输出到单独的文件和 LOG(ERROR)
void SignalHandle(const char* data, int size)
{
    std::string str = std::string(data,size);
    /*
    std::ofstream fs("glog_dump.log",std::ios::app);
    fs<<str;
    fs.close();
    */
    LOG(ERROR) << str;
}

GLogHelper::GLogConfig::GLogConfig()
{
    program = "";
    FLAGS_logtostderr = false; //--logtostderr=1, GLOG_logtostderr=1
    FLAGS_colorlogtostderr = true;  //设置输出到屏幕的日志显示相应颜色
    FLAGS_stderrthreshold = google::GLOG_WARNING;  //google::SetStderrLogging(google::WARNING); //设置级别高于 google::INFO 的日志同时输出到屏幕
    FLAGS_minloglevel = google::GLOG_INFO;
    FLAGS_logbuflevel = -1;
    FLAGS_logbufsecs = 0;   //缓冲日志输出，默认为30秒，此处改为立即输出
    FLAGS_logfile_mode = 0664;
    FLAGS_max_log_size = 25;    //最大日志大小为 25MB
    FLAGS_stop_logging_if_full_disk = true; //当磁盘被写满时，停止日志输出
    FLAGS_v = 0;
    LogDir = GLOG_FILE_PATH;
    InfoLogFileNameBase = "LOG-";
    LogFileNameExtension = "SDV-";
}

GLogHelper::GLogConfig::~GLogConfig()
{
}

GLogHelper::GLogHelper(GLogConfig& config)
{
    struct stat st;
    if (stat(config.LogDir.c_str(), &st) < 0) {
        if (mkdir(config.LogDir.c_str(), 0755) < 0) {
            LOG(ERROR) << "create '" << config.LogDir << "' failed: " <<  strerror(errno);
        }
    }

    google::InitGoogleLogging(config.program.c_str());

    FLAGS_logtostderr = config.FLAGS_logtostderr;
    FLAGS_colorlogtostderr = config.FLAGS_colorlogtostderr;
    FLAGS_stderrthreshold = config.FLAGS_stderrthreshold;
    FLAGS_minloglevel = config.FLAGS_minloglevel;
    FLAGS_logbuflevel = config.FLAGS_logbuflevel;
    FLAGS_logbufsecs = config.FLAGS_logbufsecs;
    FLAGS_logfile_mode = config.FLAGS_logfile_mode;
    FLAGS_max_log_size = config.FLAGS_max_log_size;
    FLAGS_stop_logging_if_full_disk = config.FLAGS_stop_logging_if_full_disk;
    FLAGS_v = config.FLAGS_v;
    
    std::string logBaseFileName = config.LogDir + '/' + config.InfoLogFileNameBase;
    google::SetLogDestination(google::GLOG_INFO, logBaseFileName.c_str());       //设置 google::INFO 级别的日志存储路径和文件名前缀
    google::SetLogDestination(google::GLOG_WARNING, ""); //"/WARNING-"   //设置 google::WARNING 级别的日志存储路径和文件名前缀
    google::SetLogDestination(google::GLOG_ERROR, "");   //"/ERROR-"     //设置 google::ERROR 级别的日志存储路径和文件名前缀
    google::SetLogDestination(google::GLOG_FATAL, "");   //"/FATAL-"     //设置 google::FATAL 级别的日志存储路径和文件名前缀
    google::SetLogFilenameExtension(config.LogFileNameExtension.c_str());     //设置文件名扩展，如平台？或其它需要区分的信息
    google::InstallFailureSignalHandler();                                  //捕捉 CoreDump
    google::InstallFailureWriter(&SignalHandle);                            //默认捕捉 SIGSEGV 信号信息输出会输出到 stderr，可以通过下面的方法自定义输出方式：
}

GLogHelper::~GLogHelper()
{
    google::ShutdownGoogleLogging();
}
