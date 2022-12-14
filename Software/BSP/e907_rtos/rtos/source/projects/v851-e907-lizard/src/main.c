#include <stdio.h>
#include <openamp/sunxi_helper/openamp.h>

extern int csi_init(int argc, const char **argv);

int app_entry(void *param)
{
#ifdef CONFIG_DRIVERS_VIN
	csi_init(0, NULL);
#if 1
	rpmsg_notify("twi1", NULL, 0);
	rpmsg_notify("isp0", NULL, 0);
	rpmsg_notify("scaler0", NULL, 0);
	rpmsg_notify("scaler4", NULL, 0);
	rpmsg_notify("scaler8", NULL, 0);
	rpmsg_notify("scaler12", NULL, 0);
	rpmsg_notify("vinc0", NULL, 0);
	rpmsg_notify("vinc4", NULL, 0);
	rpmsg_notify("vinc8", NULL, 0);
	rpmsg_notify("vinc12", NULL, 0);
#endif
#endif
    return 0;
}
