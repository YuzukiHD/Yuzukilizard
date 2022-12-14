/*
 * this file add the notify of pm, when enter standby, the drivers for example
 * cpu resource of cache ,timer, interrupt, float can control to save
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <melis/common/list.h>
#include <melis/log.h>
#include <melis/ekernel/drivers/sys_fsys.h>
#include <errno.h>
#include <melis/standby/standby.h>

LIST_HEAD(list_pm_notify_dev);

struct dev_pm {
	struct list_head list;
	int (*suspend)(void *);
	int (*resume)(void *);
	void *d;

};

struct dev_pm *register_pm_dev_notify(int (*suspend)(void *),
				      int (*resume)(void *), void *d)
{
	struct dev_pm *pm = malloc(sizeof(struct dev_pm));
	if (!pm) {
		pr_err("pm notify:get mem failed\n");
		return ERR_PTR(-ENOMEM);
	}

	pm->resume = resume;
	pm->suspend = suspend;
	pm->d = d;

	list_add(&pm->list, &list_pm_notify_dev);

	return pm;
}

void unregister_pm_dev_notify(struct dev_pm *pm)
{
	list_del(&pm->list);

	free(pm);
}

int dev_pm_notify_call(enum dev_notify_type type)
{
	struct dev_pm *pm;
	int rst = 0;

	switch (type) {
	case DEV_NOTIFY_RESUME:
		list_for_each_entry_reverse(pm, &list_pm_notify_dev, list) {
			if (pm->resume)
				rst = pm->resume(pm->d);

			if (rst) {
				pr_err("dev_notify: error resume\n");
				return rst;
			}
		}
		break;
	case DEV_NOTIFY_SUSPEND:
		list_for_each_entry(pm, &list_pm_notify_dev, list) {
			if (pm->suspend)
				rst = pm->suspend(pm->d);

			if (rst) {
				pr_err("dev_notify: error suspend\n");
				return rst;
			}
		}
		break;
	}

	return 0;
}

