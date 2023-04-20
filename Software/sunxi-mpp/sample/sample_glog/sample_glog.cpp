/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_face_detect.c
  Version       : V6.0
  Author        : Allwinner BU3-XIAN Team
  Created       :
  Last Modified : 2017/11/30
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//#include <pthread.h>
//#include <assert.h>
//#include <time.h>

#if 0 //use glog directly
#include <glog/logging.h>
int main(int argc, char* argv[]) 
{
    printf("hello, world! [%s][%s][%s][%d][%s].\n", __DATE__, __TIME__, __FILE__, __LINE__, __FUNCTION__);
    // Initialize Google's logging library.
    int i;
    for(i=0;i<argc;i++)
    {
        printf("[%d]:[%s]\n", i, argv[i]);
    }
    //"If specified, logfiles are written into this directory instead of the default logging directory."
    //FLAGS_log_dir = "";
    google::InitGoogleLogging(argv[0]);

    //"log messages go to stderr instead of logfiles"
    FLAGS_logtostderr = false;  //--logtostderr=1, GLOG_logtostderr=1
    //"color messages logged to stderr (if supported by terminal)"
    FLAGS_colorlogtostderr = true;
    //"log messages at or above this level are copied to stderr in addition to logfiles.  This flag obsoletes --alsologtostderr."
    FLAGS_stderrthreshold = google::GLOG_INFO;
    //"Messages logged at a lower level than this don't actually get logged anywhere"
    FLAGS_minloglevel = google::GLOG_INFO;
    //"Buffer log messages logged at this level or lower (-1 means don't buffer; 0 means buffer INFO only;...)"
    FLAGS_logbuflevel = -1;
    //"Buffer log messages for at most this many seconds"
    FLAGS_logbufsecs = 0;
    //"Log file mode/permissions."
    FLAGS_logfile_mode = 0664;
    //"approx. maximum log file size (in MB). A value of 0 will be silently overridden to 1."
    FLAGS_max_log_size = 2;
    //"Stop attempting to log to disk if the disk is full."
    FLAGS_stop_logging_if_full_disk = true;
    //"Show all VLOG(m) messages for m <= this. Overridable by --vmodule."
    FLAGS_v = 1;

    google::SetLogDestination(google::GLOG_INFO, "/tmp/LOG-");
    google::SetLogDestination(google::GLOG_WARNING, "");
    google::SetLogDestination(google::GLOG_ERROR, "");
    google::SetLogDestination(google::GLOG_FATAL, "");
    google::SetLogFilenameExtension("SDV-");

    int num_cookies = 0;
    LOG(INFO) << "info Found " << num_cookies << " cookies\n";
    LOG(WARNING) << "warning Found " << num_cookies << " cookies\n";
    LOG(ERROR) << "error Found " << num_cookies << " cookies\n";
    //LOG(FATAL) << "fatal Found " << num_cookies << " cookies\n";
    VLOG(1) << "I'm printed when you run the program with --v=1 or higher";
    VLOG(2) << "I'm printed when you run the program with --v=2 or higher";

        
    //google::TruncateLogFile(const char *path, int64 limit, int64 keep);
    google::ShutdownGoogleLogging();

    //LOG(INFO) << "info log";
    //LOG(WARNING) << "warning log";
    //LOG(ERROR) << "ShutdownGoogleLogging!";

    return 0;
}

#else  //use glog_helper which wraps glog.
#include <plat_log.h>
void verylonglonglonglonglonglonglongname()
{
    aloge("This is a very long name!");
}

int main(int argc, char* argv[])
{
    int result = 0;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 0,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 25,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "SDV-");
    
    log_init(argv[0], &stGLogConfig);
    alogv("v, hello, world!");
    alogd("d, hello, world!");
    alogw("w, hello, world!");
    aloge("e, hello, world!");
    verylonglonglonglonglonglonglongname();

    alogd("123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,\n"
        "123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,\n"
        "123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789,123456789");
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
#endif
