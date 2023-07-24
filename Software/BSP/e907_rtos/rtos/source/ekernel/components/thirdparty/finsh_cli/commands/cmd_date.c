#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <hal_cmd.h>

static void show_help(void)
{
    printf("Usage: date [year-month-day hour:min:sec]\n");
    printf("       date 2018-09-11 01:22:01\n");
}

static int getdayofmonth(int year, int month)
{
    if (month < 1 || month > 12)
    {
        return 0;
    }

    int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    days[1] = ((year % 4 == 0 && year % 100) || year % 400) == 0 ? 29 : 28;

    return days[month-1];
}

static int cmd_date(int argc, const char **argv)
{
    struct timeval tm_val;
    struct tm tm;
    int ch = 0;
    int ret = 0;
    time_t time;

    char timestamp[50];

    if (argc == 1)
    {
        memset(timestamp, 0, sizeof(timestamp));
        memset(&tm_val, 0, sizeof(struct timeval));

        gettimeofday(&tm_val, NULL);
        localtime_r(&tm_val.tv_sec, &tm);
        sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec,
                tm_val.tv_usec / 1000);

        printf("%s\n", timestamp);
        return ret;
    }

    while ((ch = getopt(argc, (char **)argv, "hs:")) != -1)
    {
        switch (ch)
        {
            case 's':
                memset(&tm, 0, sizeof(struct tm));
                ret = sscanf(optarg, "%04d-%02d-%02d %02d:%02d:%02d",
                             &tm.tm_year,
                             &tm.tm_mon,
                             &tm.tm_mday,
                             &tm.tm_hour,
                             &tm.tm_min,
                             &tm.tm_sec);

                if (ret != 6)
                {
                    show_help();
                    return -1;
                }
                else
                {
                    if ((tm.tm_year < 1970)
                        || (tm.tm_mon < 1 || tm.tm_mon > 12)
                        || (tm.tm_mday < 1 || tm.tm_mday > getdayofmonth(tm.tm_year, tm.tm_mon))
                        || (tm.tm_hour < 0 || tm.tm_hour > 23)
                        || (tm.tm_min < 0 || tm.tm_min > 59)
                        || (tm.tm_sec < 0 || tm.tm_sec > 59))
                    {
                        show_help();
                        return -1;
                    }
                    tm.tm_year -= 1900;
                    tm.tm_mon -= 1;

                    time = mktime(&tm);

                    struct timeval sys_time = {0};
                    sys_time.tv_sec = time;
                    settimeofday(&sys_time, NULL);
                }
                break;
            case 'h':
                show_help();
                break;
            default:
                break;
        }
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_date, __cmd_date, See now date);
