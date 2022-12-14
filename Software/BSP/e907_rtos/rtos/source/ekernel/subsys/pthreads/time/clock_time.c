/*
 * File      : clock_time.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2012, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-12-08     Bernard      fix the issue of _timevalue.tv_usec initialization,
 *                             which found by Rob <rdent@iinet.net.au>
 */

#include <rtthread.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include "clock_time.h"

static struct timeval _timevalue;
int clock_time_system_init(void)
{
    time_t time;
    rt_tick_t tick;
    rt_device_t device;

    time = 0;
    device = rt_device_find("rtc");
    if (device != RT_NULL)
    {
        /* get realtime seconds */
        rt_device_control(device, RT_DEVICE_CTRL_RTC_GET_TIME, &time);
    }

    /* get tick */
    tick = rt_tick_get();

    _timevalue.tv_usec = (tick % RT_TICK_PER_SECOND) * MICROSECOND_PER_TICK;
    _timevalue.tv_sec = time - tick / RT_TICK_PER_SECOND - 1;

    return 0;
}
INIT_COMPONENT_EXPORT(clock_time_system_init);

int clock_time_to_tick(const struct timespec *time)
{
    int tick;
    long long nsecond, second;
    struct timespec tp;

    RT_ASSERT(time != RT_NULL);

    tick = RT_WAITING_FOREVER;
    if (time != NULL)
    {
        /* get current tp */
        clock_gettime(CLOCK_REALTIME, &tp);

        if ((time->tv_nsec - tp.tv_nsec) < 0)
        {
            nsecond = NANOSECOND_PER_SECOND - (tp.tv_nsec - time->tv_nsec);
            second  = time->tv_sec - tp.tv_sec - 1;
        }
        else
        {
            nsecond = time->tv_nsec - tp.tv_nsec;
            second  = time->tv_sec - tp.tv_sec;
        }

        tick = second * RT_TICK_PER_SECOND + nsecond * RT_TICK_PER_SECOND / NANOSECOND_PER_SECOND;
        if (tick < 0)
        {
            tick = 0;
        }
    }

    return tick;
}
RTM_EXPORT(clock_time_to_tick);

int clock_getres(clockid_t clockid, struct timespec *res)
{
    int ret = 0;

    if (res == RT_NULL)
    {
        rt_set_errno(EINVAL);
        return -1;
    }

    switch (clockid)
    {
        case CLOCK_REALTIME:
            res->tv_sec = 0;
            res->tv_nsec = NANOSECOND_PER_SECOND / RT_TICK_PER_SECOND;
            break;

#ifdef RT_USING_CPUTIME
        case CLOCK_CPUTIME_ID:
            res->tv_sec  = 0;
            res->tv_nsec = clock_cpu_getres();
            break;
#endif

        default:
            ret = -1;
            rt_set_errno(EINVAL);
            break;
    }

    return ret;
}
RTM_EXPORT(clock_getres);

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
    int ret = 0;

    if (tp == RT_NULL)
    {
        rt_set_errno(EINVAL);
        return -1;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long monotonic_time_ms = 0;

    switch (clockid)
    {
        case CLOCK_REALTIME:
        {
            tp->tv_sec  = tv.tv_sec;
            tp->tv_nsec = tv.tv_usec * 1000UL;
        }
        break;

        case CLOCK_MONOTONIC:
        {
            monotonic_time_ms = rt_tick_get() * (1000 / RT_TICK_PER_SECOND);
            tp->tv_sec = monotonic_time_ms / 1000;
            tp->tv_nsec = (monotonic_time_ms % 1000) * 1000L * 1000L;
        }
        break;

#ifdef RT_USING_CPUTIME
        case CLOCK_CPUTIME_ID:
        {
            float unit = 0;
            long long cpu_tick;

            unit = clock_cpu_getres();
            cpu_tick = clock_cpu_gettime();

            tp->tv_sec  = ((int)(cpu_tick * unit)) / NANOSECOND_PER_SECOND;
            tp->tv_nsec = ((int)(cpu_tick * unit)) % NANOSECOND_PER_SECOND;
        }
        break;
#endif
        default:
            rt_set_errno(EINVAL);
            ret = -1;
    }

    return ret;
}
RTM_EXPORT(clock_gettime);

int clock_settime(clockid_t clockid, const struct timespec *tp)
{
    if (tp == RT_NULL)
    {
        rt_set_errno(EINVAL);

        return -1;
    }

   struct timeval tv;
    switch (clockid) {
        case CLOCK_REALTIME:
            tv.tv_sec = tp->tv_sec;
            tv.tv_usec = tp->tv_nsec / 1000L;
            settimeofday(&tv, NULL);
            break;
        default:
            printf("Do not support set monotonic time by clock_settime\n");
            rt_set_errno(EINVAL);
            return -1;
    }

    return 0;
}
RTM_EXPORT(clock_settime);
