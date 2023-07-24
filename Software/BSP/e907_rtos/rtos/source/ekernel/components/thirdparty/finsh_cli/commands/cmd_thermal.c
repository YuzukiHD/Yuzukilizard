#include <rtthread.h>
#include <cli_console.h>
#include "msh.h"
#include <finsh.h>
#include <finsh_api.h>
#include <shell.h>
#include <mod_defs.h>
#include <mod_update.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sunxi_drv_thermal.h>
#include "unistd.h"


int cmd_ths_gt_status(int argc, char **argv)
{
	temperature_control_strategy_t strategy = TEMPERATURE_CONTROL_NONE;
	rt_device_t dev;

	if (argc < 2)
	{
		strategy = TEMPERATURE_CONTROL_NONE;
	}
	else {
		if (*argv[1] == '1')
		{
			strategy = TEMPERATURE_CONTROL_STANDBY;
		}
		else
		{
			strategy = TEMPERATURE_CONTROL_NONE;
		}
	}
	dev = rt_device_find("thermal");
	if (rt_device_open(dev, RT_DEVICE_FLAG_RDWR))
	{
		return -1;
	}
	dev->control(dev, THERMAL_SET_STRATEGY, &strategy);
	rt_device_close(dev);
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_ths_gt_status, ths_set_s, thermal set strategy)

