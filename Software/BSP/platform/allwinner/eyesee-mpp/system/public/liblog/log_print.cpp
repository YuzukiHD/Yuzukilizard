#include "log/log_print.h"
#include "log/glog_helper.h"

#include <stdarg.h>
#include <iostream>
#include <iomanip>

#ifndef FORMAT_LOG
#define FORMAT_LOG                  1
#endif

/* position */
#if FORMAT_LOG
#define XPOSTO(x)   "\033[" #x "D\033[" #x "C"
#else
#define XPOSTO(x) ""
#endif

static GLogHelper *gh = NULL;

void log_init(const char *program, GLogConfig *pConfig)
{
    if (gh) return;

    GLogHelper::GLogConfig *pGLogHelperConfig = new GLogHelper::GLogConfig;
    pGLogHelperConfig->program = program;
    if(pConfig)
    {
        pGLogHelperConfig->FLAGS_logtostderr = pConfig->FLAGS_logtostderr;
        pGLogHelperConfig->FLAGS_colorlogtostderr = pConfig->FLAGS_colorlogtostderr;
        pGLogHelperConfig->FLAGS_stderrthreshold = pConfig->FLAGS_stderrthreshold;
        pGLogHelperConfig->FLAGS_minloglevel = pConfig->FLAGS_minloglevel;
        pGLogHelperConfig->FLAGS_logbuflevel = pConfig->FLAGS_logbuflevel;
        pGLogHelperConfig->FLAGS_logbufsecs = pConfig->FLAGS_logbufsecs;
        //pGLogHelperConfig->FLAGS_logfile_mode
        pGLogHelperConfig->FLAGS_max_log_size = pConfig->FLAGS_max_log_size;
        pGLogHelperConfig->FLAGS_stop_logging_if_full_disk = pConfig->FLAGS_stop_logging_if_full_disk;
        //pGLogHelperConfig->FLAGS_v
        pGLogHelperConfig->LogDir = pConfig->LogDir;
        pGLogHelperConfig->InfoLogFileNameBase = pConfig->InfoLogFileNameBase;
        pGLogHelperConfig->LogFileNameExtension = pConfig->LogFileNameExtension;
    }
    
    gh = new GLogHelper(*pGLogHelperConfig);

    delete pGLogHelperConfig;
}

void log_quit()
{
    if (gh) {
        delete gh;
        gh = NULL;
    }
}

int log_printf(const char *file, const char *func, int line, const int level, const char *format, ...)
{
    int result = 0;
    char ChBuffer[128] = {0};
    char *buffer = NULL;
    char *p = NULL;
    int n;
    int size = sizeof(ChBuffer);
    va_list args;

    va_start(args, format);
    n = vsnprintf(ChBuffer, size, format, args);
    va_end(args);

    /* If that worked, use the string. */
    if (n > -1 && n < size)
    {
        //printf("[1]n=%d, size=%d\n", n, size);
        buffer = ChBuffer;
    }
    /* Else try again with more space. */
    else if(n > -1)
    {
        size = n+1; /* precisely what is needed */ 
        if ((p = (char*)malloc(size)) == NULL)
        {
            printf("(f:%s, l:%d) fatal error! n=%d. use previous buffer\n", __FUNCTION__, __LINE__, n);
            buffer = ChBuffer;
        }
        else
        {
            //printf("[2]n=%d, size=%d\n", n, size);
            va_start(args, format);
            n = vsnprintf(p, size, format, args);
            va_end(args);
            if (n > -1 && n < size)
            {
                buffer = p;
            }
            else
            {
                printf("(f:%s, l:%d) fatal error! n=%d. use previous buffer\n", __FUNCTION__, __LINE__, n);
                buffer = ChBuffer;
            }
        }
    }
    else
    {
        printf("(f:%s, l:%d) fatal error! n=%d. use previous buffer\n", __FUNCTION__, __LINE__, n);
        buffer = ChBuffer;
    }

    if (level == _GLOG_INFO)
        google::LogMessage(file, line).stream()
            << XPOSTO(60)
            << "<" << func << "> "
            //<< XPOSTO(90)
            << buffer;
    else if (level == _GLOG_WARN)
        google::LogMessage(file, line, google::GLOG_WARNING).stream()
            << XPOSTO(60)
            << "<" << func << "> "
            //<< XPOSTO(90)
            << buffer;
    else if (level == _GLOG_ERROR)
        google::LogMessage(file, line, google::GLOG_ERROR).stream()
            << XPOSTO(60)
            << "<" << func << "> "
            //<< XPOSTO(90)
            << buffer;
    else if (level == _GLOG_FATAL)
        google::LogMessage(file, line, google::GLOG_FATAL).stream()
            << XPOSTO(60)
            << "<" << func << "> "
            //<< XPOSTO(90)
            << buffer;
    else
        google::LogMessage(file, line).stream()
            << XPOSTO(60)
            << "<" << func << "> "
            //<< XPOSTO(90)
            << buffer;

    if(p)
    {
        free(p);
        p = NULL;
    }

    return result;
}
