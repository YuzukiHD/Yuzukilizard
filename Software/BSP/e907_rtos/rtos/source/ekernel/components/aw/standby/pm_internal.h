/*
 * pm_internel.h
 */

#ifndef __PM_INTERNAL_H
#define __PM_INTERNAL_H

#include <melis/standby/standby.h>

int dev_pm_notify_call(enum dev_notify_type type);
void irq_resume(void);
void irq_suspend(void);

#endif /* __PM_INTERNAL_H */
