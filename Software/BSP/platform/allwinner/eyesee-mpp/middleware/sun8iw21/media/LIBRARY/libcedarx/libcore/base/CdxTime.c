/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxTime.c
 * Description : Time
 * History :
 *
 */

#include <CdxTime.h>

#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

cdx_int64 CdxGetNowUs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (cdx_int64)tv.tv_usec + tv.tv_sec * 1000000ll;
}

cdx_int64 CdxMonoTimeUs(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        abort();

    return (1000000LL * ts.tv_sec) + (ts.tv_nsec / 1000);
}
