#include <pthread.h>
#include <sys/file.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdarg.h>

#include "log_handle.h"

static FILE                  *g_log_file = NULL;
static pthread_mutex_t       g_log_locker;
static int                   g_log_locker_init_flag = 0;  // 0 - not init, 1 - inited

static char                  g_log_buffer[512];
static int                   g_log_buffer_length;
static struct timespec       g_log_time;
static char                  g_log_time_buffer[32];


int init_log(const char *log_file, const char *mode)
{	
	if (!log_file || !mode) {
    	return -1;
    }
	
	if (!g_log_locker_init_flag) {
		pthread_mutex_init(&g_log_locker, NULL);
		g_log_locker_init_flag = 1;
	}

	if (g_log_file) {
		fclose(g_log_file);
		g_log_file = NULL;
	}

	g_log_file = fopen(log_file, mode);
	if (!g_log_file) {
		printf("%s: failed to open %s\n", __FUNCTION__, log_file);
		return -2;
	}
	
	return 0;
}

void write_log(const char *format, ...)
{
    if (format && g_log_file && g_log_locker_init_flag) {
		pthread_mutex_lock(&g_log_locker);
		g_log_buffer_length = sprintf(g_log_buffer, "%s", get_sys_time_label());

		va_list ap;
		va_start(ap, format);
		g_log_buffer_length += vsprintf((g_log_buffer + g_log_buffer_length), format, ap);
		va_end(ap);
			
		fwrite(g_log_buffer, g_log_buffer_length, 1, g_log_file);
		fflush(g_log_file);
		pthread_mutex_unlock(&g_log_locker);
	}
}

void close_log()
{
	if (g_log_locker_init_flag) {
		pthread_mutex_lock(&g_log_locker);
		if (g_log_file) {
			fclose(g_log_file);
			g_log_file = NULL;
    	}
		pthread_mutex_unlock(&g_log_locker);
		pthread_mutex_destroy(&g_log_locker);
		g_log_locker_init_flag = 0;
	}
}

const char *get_sys_time_label()
{
	clock_gettime(CLOCK_MONOTONIC, &g_log_time);
	sprintf(g_log_time_buffer, "<%6lu.%09lu>", g_log_time.tv_sec, g_log_time.tv_nsec);
	return g_log_time_buffer;
}

void get_sys_time(char *time_str, const char *format)
{
	time_t t_sec;
	struct tm *local;

	if (time_str) {
		t_sec = time(NULL);
		local = localtime(&t_sec);
		if (format) {
			sprintf(time_str, format,
				local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
				local->tm_hour, local->tm_min, local->tm_sec);
		} else {
			sprintf(time_str, "%04d-%02d-%02d_%02d:%02d:%02d",
				local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
				local->tm_hour, local->tm_min, local->tm_sec);
		}
	}
}


