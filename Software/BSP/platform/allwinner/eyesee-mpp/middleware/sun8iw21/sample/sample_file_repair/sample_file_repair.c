#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <utils/plat_log.h>
#include "log/log_wrapper.h"
#include "file_repair.h"
int main(int argc, char *argv[])
{
    char *p_file_path = argv[1];
    int ret = 0;
    if(NULL == p_file_path)
    {
        aloge("file_name_invalid\n");
        return 0;
    }

    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 0,
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
    strcpy(stGLogConfig.LogFileNameExtension, "v833-");
    log_init(argv[0], &stGLogConfig);

    aloge("file_to_be_repaired:%s\n",p_file_path);

    ret = mp4_file_repair(p_file_path);

    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;

}
