#ifndef _STORAGE_MANAGER_H_
#define _STORAGE_MANAGER_H_

#define SDCARD_DEV "/dev/mmcblk0p1"
#define SDCARD_DEV_2 "/dev/mmcblk0"
#define SDCARD_MOUNT_PATH "/mnt/extsd"


int sd_card_format_func();
int storage_manager_init(void);
int storage_manager_exit(void);
int storage_manager_check_mount(const char *mount_path, const char *dev_path);
int storage_manager_mount(const char *mount_path, const char *dev_path);
int storage_manager_umount(const char *mount_path);
int storage_manager_format(const char *dev_path);
int storage_manager_get_dir_capacity(const char *dir,
				     unsigned int *avail,
				     unsigned int *total);
/*input : fix=1, check fat file sytem and fix when is wrong; fix=0, dont fix*/
/*return  >=0 :are support , <0:not support this systemfile*/
int storage_manger_check_and_fix_filesystem_support(int fix);

/*checing sd card was be checking??*/
/*input: fb is a callback func, it will be run before reboot*/
int enable_sdcard_staus_checking(void *fb);
int disable_sdcard_staus_checking(void);

#endif /*_STORAGE_MANAGER_H_*/
