#include <shell.h>
#include <mod_defs.h>
#include <mod_update.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hal_timer.h>
#include <hal_cmd.h>
#ifdef CONFIG_DRIVER_WDT
#include <drv/sunxi_drv_watchdog.h>
#endif
static void show_help(void)
{
    printf("Usage: Update -f: erase boot0 force usb update\n");
    printf("Usage: Update [File_Path]\n");
}

int cmd_update(int argc, char *argv[])
{
	__u8 Mid_Update = 0;
	__mp *Mod_Update = NULL;
	int prog = 0;	
	FILE *File = 0;
	__u32 find_file = 0;
	__u32 force_update = 0;

	if (argc != 2)
	{
		show_help();
		return 0;
	}

	if (strcmp(argv[1], "-f") != 0)
	{
		File = fopen(argv[1], "rb");
		if (File == NULL) {
			printf("-------File Path Not Exist:%s--------!\n", argv[1]);
			show_help();
			return 0;
		}
		else
		{
			printf("-------File Path Exist:%s--------!\n", argv[1]);
		}
		fclose(File);
	} else {
		force_update = 1;
	}
	Mid_Update = esMODS_MInstall("/mnt/D/mod/update.mod", 0);
//    printf("-------cmd_update Mid_Update:0x%x--------!\n", (unsigned int)Mid_Update);
    if (!Mid_Update)
    {
        printf("install update fail!");
        return -1;
    }
    /*open update*/
    Mod_Update = esMODS_MOpen(Mid_Update, 0);
    if (Mod_Update == NULL)
    {
        printf("open update fail!");
        return -1;
    }
	if (force_update)
	{
	    esMODS_MIoctrl(Mod_Update, UPDATE_CMD_FORCE_USB_UPDATE, 0, 0);
#ifdef CONFIG_DRIVER_WDT
		printf("please connect usb to pc, machine will reboot to burn new firmware!\n");
		hal_sleep(2);
		reset_cpu();
#else		
       	printf("please connect usb to pc, and reboot the machine!\n");
#endif
	} else {
	    esMODS_MIoctrl(Mod_Update, UPDATE_CMD_START, 0, argv[1]);
		while (1){
			hal_sleep(1);
			prog = esMODS_MIoctrl(Mod_Update, UPDATE_CMD_CHECK_PROG, 0, 0);
	        printf("update: ...[%02d]...\n", prog);
			if (prog == 100)
			{
#ifndef CONFIG_DRIVER_WDT
				printf("update finish please reboot the machine!\n");
#else
				printf("update finish,will reboot the machine!\n");
				hal_sleep(2);
				reset_cpu();
#endif
				break;
			}
		}
	}
    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_update, update, update system);
