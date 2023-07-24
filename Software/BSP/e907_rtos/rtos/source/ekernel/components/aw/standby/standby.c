/*
 * standby enter function
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <melis/standby/standby.h>
#include "pm_internal.h"

#include <hal_atomic.h>

void (*arch_enter_standby)(int) = NULL;
void (*arch_core_suspend)(void) = NULL;
void (*arch_core_resume)(void) = NULL;

int standby_enter(int level)
{
	int err = 0;

	hal_enter_critical();
	dev_pm_notify_call(DEV_NOTIFY_SUSPEND);

	printf("PM: standby enter\n");
	irq_suspend();

	if (arch_enter_standby) {

		if (arch_core_suspend)
			arch_core_suspend();

		arch_enter_standby(level);

		if (arch_core_resume)
			arch_core_resume();
	}

	irq_resume();
	printf("PM: standby exit\n");

	dev_pm_notify_call(DEV_NOTIFY_RESUME);
	hal_exit_critical();

	return err;
}
