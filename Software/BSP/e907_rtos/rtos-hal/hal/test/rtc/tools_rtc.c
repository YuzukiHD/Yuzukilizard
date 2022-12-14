#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <osal/hal_mem.h>
#include <hal_log.h>
#include <hal_cmd.h>
#include <sunxi_hal_rtc.h>
#include <console.h>


static int show_time(struct rtc_time *rtc_tm, const char *str)
{
	printf("%s%04d-%02d-%02d %02d:%02d:%02d\n", str,
		   rtc_tm->tm_year + 1900, rtc_tm->tm_mon + 1, rtc_tm->tm_mday,
		   rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);
	return 0;
}


int store_time(struct rtc_time *tm, const char *str)
{
	int ret = 0;
	struct rtc_time tmp;

	if (!tm || !str || strlen(str) < 19) {
		printf("invalid parameter.\n");
		return -1;
	}

	ret = sscanf(str, "%04d-%02d-%02d %02d:%02d:%02d",
		   &tmp.tm_year, &tmp.tm_mon, &tmp.tm_mday,
		   &tmp.tm_hour, &tmp.tm_min, &tmp.tm_sec);

	if (ret != 6 || tmp.tm_year<1900 || tmp.tm_mon<1) {
		printf("ret: %d\n", ret);
		show_time(&tmp, "phase: ");
		return -1;
	}

	tmp.tm_year -= 1900;
	tmp.tm_mon  -= 1;

	show_time(&tmp, "phase: ");
	memcpy(tm, &tmp, sizeof(struct rtc_time));

	return 0;
}


#if 0
char *alarm_ptr;
int alarm_callback(void)
{
	if (alarm_ptr)
		printf("%s\n", alarm_ptr);

	return 0;
}

static int cmd_rtcinit(int argc, char **argv)
{
	int len = 0;

	hal_rtc_init();

	if (argc == 2) {
		len = strlen(argv[1]);
		len = len <  0 ? 0  : len;
		len = len > 63 ? 63 : len;

		if (alarm_ptr)
			hal_free(alarm_ptr);

		alarm_ptr = hal_malloc(len + 1);
		if (alarm_ptr) {
			memset(alarm_ptr, 0, len+1);
			memcpy(alarm_ptr, argv[1], len);
		}
	}

	printf("callback: %s\n", alarm_ptr?alarm_ptr:"NULL");
	hal_rtc_register_callback(alarm_callback);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rtcinit,  rtcinit,  rtcinit in rtc tools)

#endif

void cmd_date_help(void)
{
	printf( "\n"
		"-h: show the message.\n"
		"-g: get rtc time.\n"
		"    date or date -g\n"
		"-s: set rtc time.\n"
		"    date -s 2020-01-02 14:05:06\n"
	);
}

static int cmd_date(int argc, char **argv)
{
	int opt;
	char   *string = "hgs:";
	struct rtc_time rtc_tm;

	if (argc == 1)
		goto show;

	while ((opt = getopt(argc, argv, string))!= -1) {
		switch (opt) {
		case 'g':
			goto show;
		case 's':
			if (store_time(&rtc_tm, optarg) < 0) {
				printf("error store time\n");
				return -1;
			}
			show_time(&rtc_tm, "Settime: ");
			if (hal_rtc_settime(&rtc_tm)) {
				printf("sunxi rtc settime error\n");
				return -1;
			}
			break;
		case 'h':
		default:
			cmd_date_help();
			return 0;
		}
	}

show:
	if (hal_rtc_gettime(&rtc_tm)) {
		printf("sunxi rtc gettime error\n");
		return -1;
	}
	show_time(&rtc_tm, "Gettime: ");

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_date,  date,  date in rtc tools)


#if 0
void cmd_alarm_help(void)
{
	printf( "\n"
		"-h: show the message\n"
		"-g: show alarm\n"
		"-s: set alarm by fixed time\n"
		"-o: set alarm by offset time\n"
	);
}

static int cmd_alarm(int argc, char **argv)
{
	int opt;
	char   *string = "hgs:o:";
	struct rtc_time rtc_tm;
	struct rtc_wkalrm wkalrm;
	int    sec = 0;

	if (argc == 1) {
		goto show;
	}

	while ((opt = getopt(argc, argv, string))!= -1) {
		switch (opt) {
		case 'g':
			goto show;
		case 's':
			if (store_time(&rtc_tm, optarg) < 0)
				return -1;
			show_time(&wkalrm.time, "Getalarm: ");
			if (hal_rtc_settime(&rtc_tm)) {
				printf("sunxi rtc settime error\n");
				return -1;
			}
			break;
		case 'o':
			sec = atoi(optarg);
			printf("sec: %d\n", sec);

			if (hal_rtc_gettime(&rtc_tm)) {
				printf("sunxi rtc gettime error\n");
				return -1;
			}

			rtc_tm.tm_sec  += sec % 60;

			rtc_tm.tm_min  += (rtc_tm.tm_sec >= 60) ? 1  : 0;
			rtc_tm.tm_sec  -= (rtc_tm.tm_sec >= 60) ? 60 : 0;

			rtc_tm.tm_min  += (sec / 60) % 60 ;

			rtc_tm.tm_hour += (rtc_tm.tm_min >= 60) ? 1  : 0;
			rtc_tm.tm_min  -= (rtc_tm.tm_min >= 60) ? 60 : 0;

			rtc_tm.tm_hour += (sec / 3600);

			if (sec < 0 || rtc_tm.tm_hour > 23) {
				show_time(&rtc_tm, "Errtime: ");
				return -1;
			}

			show_time(&rtc_tm, "WillSetAlarmtime: ");
			wkalrm.enabled = 1;
			memcpy(&wkalrm.time, &rtc_tm, sizeof(struct rtc_time));
			if (hal_rtc_setalarm(&wkalrm)) {
				printf("sunxi rtc setalarm error\n");
				return -1;
			}

			hal_rtc_alarm_irq_enable(1);
			break;

		case 'h':
		default:
			cmd_alarm_help();
			return 0;
		}
	}

show:
	if (hal_rtc_getalarm(&wkalrm)) {
		printf("sunxi rtc getalarm error\n");
		return -1;
	}
	show_time(&wkalrm.time, "Getalarm: ");
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_alarm, alarm, alarm in rtc tools)
#endif

