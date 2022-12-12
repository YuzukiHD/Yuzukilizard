
/*
 ******************************************************************************
 *
 * server.c
 *
 * Hawkview ISP - server.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/05/11	VIDEO INPUT
 *
 *****************************************************************************
 */

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <linux/spi/spidev.h>
#include <pthread.h>

#include "device/isp_dev.h"
#include "isp_dev/tools.h"

#include "isp_events/events.h"
#include "isp_tuning/isp_tuning_priv.h"
#include "isp_manage.h"

#include "iniparser/src/iniparser.h"


#include "isp.h"

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
	int isp_id = 0, isp_num = 2;

	if (argc == 1) {
		isp_id = 0;
	} else if (argc == 2) {
		isp_id = atoi(argv[1]);
	} else {
		printf("please select the isp_id: 0~1 ......\n");
		scanf("%d", &isp_id);
	}

	media_dev_init();

	if (isp_id >= 2) {
		for (isp_id = 0; isp_id < isp_num; isp_id++) {
			isp_init(isp_id);
			isp_run(isp_id);
		}

		for (isp_id = 0; isp_id < isp_num; isp_id++) {
			isp_pthread_join(isp_id);
			isp_exit(isp_id);
		}
	} else {
		isp_init(isp_id);
		isp_run(isp_id);
		isp_pthread_join(isp_id);
		isp_exit(isp_id);
	}

	media_dev_exit();

	return 0;
}
