#ifndef _LOG_HANDLE_H_V100_
#define _LOG_HANDLE_H_V100_

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define DO_LOG_EN            1
#define LOG_TYPE_CONSOLE     0xfefdfcf1
#define LOG_TYPE_WRITE_FILE  0xfefdfcf2
#define LOG_TYPE             LOG_TYPE_CONSOLE

#if DO_LOG_EN
#if (LOG_TYPE_WRITE_FILE == LOG_TYPE)
#define INIT_LOG(log, mode)  init_log(log, mode);
#define CLOSE_LOG()          close_log();
#define LOG(format, ...)     write_log(format, ##__VA_ARGS__);
#else
#define INIT_LOG(log, mode)
#define CLOSE_LOG()
#define LOG(format, ...)     printf("%s"format, get_sys_time_label(), ##__VA_ARGS__);
#endif
#else
#define INIT_LOG(log, mode)
#define CLOSE_LOG()
#define LOG(format, ...)
#endif


/*
 * init log file
 * returns 0 if ok, <0 if something went wrong
 */
int init_log(const char *log_file, const char *mode);

/*
 * write log
 */
void write_log(const char *format, ...);

/*
 * close  log
 */
void close_log();

/*
 * get time label from system starts
 * returns format "<%6d.%09d>"
 */
const char *get_sys_time_label();

/*
 * get system time
 * default format: "%04d-%02d-%02d_%02d:%02d:%02d"
 */
void get_sys_time(char *time_str, const char *format);

#endif /* _LOG_HANDLE_H_V100_ */

