#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/vfs.h>
#include <stdint.h>
//#define INFO_DEBUG

#ifdef INFO_DEBUG
#define DB_PRT(fmt, args...)                                          \
    do {                                                                  \
        printf("[FUN]%s [LINE]%d  " fmt, __FUNCTION__, __LINE__, ##args); \
    } while (0)
#else
#define DB_PRT(fmt, args...)                                          \
    do {} while (0)
#endif

#define ERR_PRT(fmt, args...)                                                                        \
    do {                                                                                                 \
        printf("\033[0;32;31m ERROR! [FUN]%s [LINE]%d  " fmt "\033[0m", __FUNCTION__, __LINE__, ##args); \
    } while (0)

#include "sdcard_manager.h"

#define MOUNT_FS_TYPE "vfat"
#define FORMAT_BIN "/usr/bin/newfs_msdos"
#define CHECK_FS_BIN1   "/usr/bin/fsck_msdos"
#define CHECK_FS_BIN2   "/sbin/fsck.fat"
#define CHECK_SDCARD_NODE "/sys/block/mmcblk0/device/cid"

struct storage_manager_s
{
    int init_flag;
    pthread_mutex_t lock;
    char *fs_type;
};

/* strcut of configure */
static struct storage_manager_s storage_manager = {
    .init_flag = 0,
    .fs_type = MOUNT_FS_TYPE,
};



/*return 1:exist, 0:not exist*/
static int file_exist(const char *path)
{
    if(access(path, F_OK) == 0)
        return 1;
    else
        return 0;
}

/*return
 * 1: sd card be inster;
 * 0: sd card not inster;
 * */
int check_sdcard_insert(void)
{
    return file_exist(CHECK_SDCARD_NODE);
}


/*return -1:erro, 0:not mout, 1: was be mount*/
int storage_manager_check_mount(const char *mount_path, const char *dev_path)
{
    const char *path = "/proc/mounts";
    FILE *fp = NULL;
    char line[256] = {0};

    if(mount_path == NULL) {
        ERR_PRT("input erro \n");
        return -1;
    }

    if(!(fp = fopen(path, "r"))) {
        ERR_PRT("fopen failed, path=%s \n", path);
        return -1;
    }

    while(fgets(line, sizeof(line), fp)) {
        if(line[0] == '/' && (strstr(line, mount_path) != NULL)) {
            fclose(fp);
            fp = NULL;
            /*check sd was be insert*/
            if(dev_path != NULL) {
                if(file_exist(dev_path))
                    return 1;
                else
                    return 0;
            }
            return 1;
        } else {
            memset(line, 0, sizeof(line));
            continue;
        }
    }

    if(fp) {
        fclose(fp);
    }

    return 0;
}

int storage_manager_mount(const char *mount_path, const char *dev_path)
{
    int ret;
    char cmd[128] = {0};

    if(dev_path == NULL || mount_path == NULL) {
        ERR_PRT("input erro, \n");
        return -1;
    }

    if(!file_exist(dev_path)) {
        ERR_PRT("mount failed, no sd card device");
        return -1;
    }

    ret = mount(dev_path, mount_path, storage_manager.fs_type, 0, "errors=continue");
    if(ret < 0) {
        ERR_PRT("mount failed %s", strerror(errno));
        return -1;
    }

    DB_PRT("mount %s to %s success ! \n", dev_path, mount_path);

    return 0;

}

int storage_manager_umount(const char *mount_path)
{
    int ret;

    if(mount_path == NULL) {
        ERR_PRT("input erro \n");
        return -1;
    }

    if(storage_manager_check_mount(mount_path, NULL) == 0) {
        DB_PRT("%s was not mount, dont need umount \n", mount_path);
        return 0;
    }

    ret = umount(mount_path);
    if(ret < 0) {
        DB_PRT("umount failed %s, try to force umount \n", strerror(errno));
        sync();
        ret = umount2(mount_path, MNT_FORCE);
        if(ret < 0)
            ERR_PRT("force umount failed, %s \n", strerror(errno));
            return -1;
    }

    DB_PRT("umount success \n");
    return 0;
}

inline static uint32_t kscale(uint32_t b, uint32_t bs)
{
        return (b * (uint64_t) bs + 1024/2) >> 10;
}

int storage_manager_get_dir_capacity(const char *dir,
                     unsigned int *avail,
                     unsigned int *total)
{
    struct statfs diskinfo;
    unsigned int total_size = 0;
    unsigned int avail_size = 0;

    if(!storage_manager_check_mount(dir, NULL)) {
        ERR_PRT("sd card not mount in %s \n", dir);
        return -1;
    }

    if(statfs(dir, &diskinfo) != -1) {
        if(total) {
            total_size = kscale(diskinfo.f_blocks, diskinfo.f_bsize);
            *total = total_size ;
            DB_PRT("total : %uBytes \n", *total);
        }

        if(avail) {
            avail_size = kscale(diskinfo.f_bavail, diskinfo.f_bsize);
            *avail = avail_size;
            DB_PRT("avail : %uBytes \n", *avail);
        }
    }else{
        ERR_PRT("get %s capacity erro \n", dir);
        return -2;
    }

    return 0;
}


int storage_manager_format(const char *dev_path)
{
    int ret;
    int status = 0xAAAA;
    char cmd[128]= {0};

    DB_PRT("formatting... \n");

    pthread_mutex_lock(&storage_manager.lock);
    DB_PRT("prepare format %s \n", dev_path);
    snprintf(cmd, sizeof(cmd), "%s %s %s %s %s %s %s", FORMAT_BIN, "-F 32",
        "-n 2", "-O allwinner", "-b 65536", "-c 128", dev_path);

    printf("format commad is :%s \n",cmd);

    status = system(cmd);
    if(WIFEXITED(status)) {
        if(WEXITSTATUS(status) == 0) {
            DB_PRT("format sucess \n");
            ret = 0;
        } else {
            ERR_PRT("format failed \n");
            ret = -1;
        }
    }else{
        ERR_PRT("foramt failed : unkonw erro \n");
        ret = -1;
    }
    pthread_mutex_unlock(&storage_manager.lock);

    return ret;
}

/*return 0:are support , -1:not support this systemfie*/
static int storage_manger_check_file_system_support(void)
{
    FILE *ptr = NULL;
    char buf_ps[128];
    char m_fileSystemType[10];

    if((ptr = popen("blkid","r"))!= NULL)
    {
        memset(m_fileSystemType,0,sizeof(m_fileSystemType));
        while(fgets(buf_ps, 128, ptr) !=NULL)
        {
            if(strstr(buf_ps, SDCARD_DEV)== NULL)
                continue;
            char *saveptr = strstr(buf_ps,":");
            char devname[128];
            memset(devname, 0, sizeof(devname));
            strncpy(devname, buf_ps, saveptr-buf_ps);
            char *tmp_result1 = strstr(buf_ps,"TYPE=\"");
            char *tmp_result2 = strstr(tmp_result1+sizeof("TYPE=\"")-1,"\"");
            memcpy(m_fileSystemType,tmp_result1+sizeof("TYPE=\"")-1,strlen(tmp_result2)-strlen(tmp_result1));
            break;
        }
        pclose(ptr);
        if( strlen(m_fileSystemType) != 0)
        {
            if(!strcmp(m_fileSystemType, MOUNT_FS_TYPE))
            {
                return 0;
            }
            else
            {
                ERR_PRT("unsupport filesystem: %s \n", m_fileSystemType);
                return -1;
            }
        }
    }
    DB_PRT("check IsSupportSystemType failed\n");
    return -1;
}

/*input : fix=1, check fat file sytem and fix when is wrong; fix=0, dont fix*/
/*return  >=0 :are support , <0:not support this systemfile*/
int storage_manger_check_and_fix_filesystem_support(int fix)
{
    /*try to fix FAT filesystem when filesystem was FAT but wrong */
    if (fix) {

        int status = 0xAAAA;
        char cmd[128] = {0};
        if (storage_manger_check_file_system_support() != 0)
        {
            return -1;
        }

        snprintf(cmd, sizeof(cmd), "%s %s %s", CHECK_FS_BIN1, "-p", SDCARD_DEV);
        status = system(cmd);
        if (WIFEXITED(status))
        {
            if (WEXITSTATUS(status) == 0)
            {
                DB_PRT("No error found \n");
                return 0;
            }
            else if (WEXITSTATUS(status) == 1)
            {
                   DB_PRT("Error fixed \n");
                   return 1;
            }
            else if (WEXITSTATUS(status) == 2)
            {
                    ERR_PRT("Usage error \n");
                return -2;
            }
        }
        else
        {
            DB_PRT("CHECK_FS_BIN failed \n");
            return -3;
        }

        return 0;

    /* dont need to try to fix FAT filesystem */
    } else {
        return storage_manger_check_file_system_support();
    }
}

int sd_card_format_func()
{
    int ret;

    /*init storage_manager*/
    storage_manager_init();

    /*checking sd was be mount, and umount it*/
    ret = storage_manager_check_mount(SDCARD_MOUNT_PATH, SDCARD_DEV);
    if(ret == 1)
    {
        printf("sd card was be mount, will umount it \n");
        ret = storage_manager_umount(SDCARD_MOUNT_PATH);
        if(ret < 0 )
        {
            printf("umount sd card fail \n");
            return -1;
        }
    }
    else if (ret == 0)
    {
        printf("sdcard was not be mount. \n");
        return -2;
    }
    else
    {
        printf("check sd card erro \n");
    }

    /*format sd-card*/
    printf("format sd card \n");
    ret = storage_manager_format(SDCARD_DEV);
    if (ret < 0) {
        printf("format sd card erro \n");
        return -3;
    }

    /* mount sd */
    ret = storage_manager_mount(SDCARD_MOUNT_PATH, SDCARD_DEV);
    if(ret < 0) {
        printf("mount sd card erro \n");
        return -4;
    }

    /* exit storage_manager */
    storage_manager_exit();

    printf("format success and return now! \n");
    return 0;
}


int storage_manager_init(void)
{
    int ret;

    if(storage_manager.init_flag) {
        return 0;
    }

    /*init lock*/
    pthread_mutex_init(&storage_manager.lock, NULL);

    storage_manager.init_flag = 1;

    return 0;
}

int storage_manager_exit(void)
{
    if(storage_manager.init_flag) {
        pthread_mutex_lock(&storage_manager.lock);
        pthread_mutex_unlock(&storage_manager.lock);
        pthread_mutex_destroy(&storage_manager.lock);
    }

    storage_manager.init_flag = 0;
    return 0;

}

#if 0
int main(int argc, char *argv[])
{
    int ret;

    /*init storage_manager*/
    storage_manager_init();

    /*checking sd was be mount, and umount it*/
    ret = storage_manager_check_mount(SDCARD_MOUNT_PATH, SDCARD_DEV);
    if(ret == 1)
    {
        printf("sd card was be mount, will umount it \n");
        ret = storage_manager_umount(SDCARD_MOUNT_PATH);
        if(ret < 0 )
        {
            printf("umount sd card fail \n");
            return -1;
        }
    }
    else if (ret == 0)
    {
        printf("sdcard was not be mount. \n");
        return -2;
    }
    else
    {
        printf("check sd card erro \n");
    }

    /*check sd file system is VFAT ?*/
    ret = storage_manger_check_and_fix_filesystem_support(1); //input 1 will fix vfat erro when vfat has some wrong.
    if(ret < 0) {

        /*format sd-card*/
        printf("format sd card \n");
        ret = storage_manager_format(SDCARD_DEV);
        if (ret < 0) {
            printf("format sd card erro \n");
            return -3;
        }

        printf("format success and return now! \n");
    }

    /* mount sd */
    ret = storage_manager_mount(SDCARD_MOUNT_PATH, SDCARD_DEV);
    if(ret < 0) {
        printf("mount sd card erro \n");
        return -4;
    }

    /* exit storage_manager */
    storage_manager_exit();


    return 0;
}


#endif
