/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxDebug.c
 * Description : Debug
 * History :
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "cdx_config.h"
#if (defined(__ANDROID__) && (CONF_ANDROID_MAJOR_VER < 5))
#include <corkscrew/backtrace.h>
#endif
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <cdx_log.h>
#include <unistd.h>

#include <CdxDebug.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "CdxDebug"

void CdxDumpThreadStack(pthread_t tid)
{
#if (defined(__ANDROID__) && (CONF_ANDROID_MAJOR_VER < 5))
    const size_t MAX_DEPTH = 32;
    pthread_t tagTid;
    pthread_t curTid = gettid();

    tagTid = (tid == -1) ? curTid : tid;

    backtrace_frame_t backtrace[MAX_DEPTH];
    ssize_t frames = unwind_backtrace_thread(tagTid, backtrace, 0, MAX_DEPTH);
    if (frames > 0)
    {
        backtrace_symbol_t backtrace_symbols[MAX_DEPTH];
        get_backtrace_symbols(backtrace, frames, backtrace_symbols);

        size_t i = (tagTid == curTid) ? 2 : 0;

        for (; i < (size_t)frames; i++)
        {
            char line[1024];
            format_backtrace_line(i, &backtrace[i], &backtrace_symbols[i],
                    line, 1024);
            CDX_LOGD("  %s\n", line);
        }
        free_backtrace_symbols(backtrace_symbols, frames);
    }
    else
    {
        CDX_LOGD("(native backtrace unavailable), tid(%lu)", tagTid);
    }
#else
    (void)tid;
    return;
#endif
}

static int CdxCommonScanDirFilter(const struct dirent *dir)
{
    if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
    {
        return 0;
    }
    return 1;
}

void CdxCallStack(void)
{
    char procTaskPath[128] = {0};
    struct dirent **namelist;
    int threadNum = 0, i = 0;

    sprintf(procTaskPath, "/proc/%d/task", getpid());
    threadNum = scandir(procTaskPath, &namelist, CdxCommonScanDirFilter, NULL);
    if (threadNum <= 0)
    {
        CDX_LOGE("no task???");
        return ;
    }

    for (i = 0; i < threadNum; i++)
    {
        pthread_t pid= (pthread_t)atol(namelist[i]->d_name);
        CDX_LOGD("------------tid(%ld)------------", (unsigned long)pid);
        CdxDumpThreadStack(pid);
        CDX_LOGD("------------tid(%ld) end------------\n\n", (unsigned long)pid);
    }

    free(namelist);
    return ;
}

