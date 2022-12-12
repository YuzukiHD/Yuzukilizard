//#define FATFS
#ifdef FATFS
#include <fat_user.h>
#endif
#include "OtaUpdate.h"
#include <unistd.h>
#include "app_log.h"
#include <string.h>
#include "ota_private.h"

#define UBOOT_LEN                  (256 << 10)
static int mPartSize[20] = {0};

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OTAUPDATE"

#ifdef FATFS
static inline int fatFileExist(const char *path)
{
    int fd = fat_open(path, FA_OPEN_EXISTING);
    if (fd >= 0) {
        fat_close(fd);
        return 1;
    }
    return 0;
}
#define UPDATE_FILE_PATH            "0:/full_img.fex"
#define FILE_EXSIST(PATH)           (fatFileExist(PATH))
#else
//#define UPDATE_FILE_PATH          "/mnt/extsd/full_img.fex"



#define FILE_EXSIST(PATH)           (!access(PATH, F_OK))
#endif
#define BOOT0_MAIGC                 "eGON.BT0"

#define BUFFER_LEN                  8

#ifdef tiger_cdr
#define FILE_BUFFER                 (8<<20)
#else
//#define FILE_BUFFER                   (16<<20)
#define FILE_BUFFER                 (18<<20)

#endif

FILE * fp;
int  flash_handle = 0;
HWND MainWnd;

//#define db_msg printf

static void *watchDogThread(void *p_ctx) {
    int fd = open("/dev/watchdog", O_RDWR);
    if(fd < 0) {
        db_msg("Watch dog device open error.");
        return NULL;
    }

    int watchdog_enable = WDIOS_DISABLECARD;
    ioctl(fd,WDIOC_SETOPTIONS, &watchdog_enable);
    int time_out = 10;  //8s, min:1, max:16
    ioctl(fd, WDIOC_SETTIMEOUT, &time_out);
    ioctl(fd,WDIOC_GETTIMEOUT, &time_out);

    watchdog_enable = WDIOS_ENABLECARD;
    int io_ret = ioctl(fd, WDIOC_SETOPTIONS, &watchdog_enable);
    ALOGV("WDIOC_SETOPTIONS enable, ret = %d", io_ret);

    while(1) {
        ioctl(fd, WDIOC_KEEPALIVE, NULL);
        //write(fd,NULL,1);
        usleep(3000000);
    }
}

static int watchDogRun() {
    pthread_t thread_id = 0;
    int err = pthread_create(&thread_id, NULL, watchDogThread, NULL);
    if(err || !thread_id) {
        db_msg("Create watch dog thread error.");
        return -1;
    }

    ALOGV("Create watch dog thread success.");
    return 0;
}

static int sd_upgrade(unsigned char mode, unsigned char *buf, unsigned int len, int part[])
{
    db_msg("*****%s,%d****",__FUNCTION__,__LINE__);
    unsigned char *upgrade_buf = NULL;
    int offset = 0;
    int ret_len = 0;
    unsigned char *tmp_buf ;
    const char *destTem;
    destTem = BOOT0_MAIGC;
    int update_flag = 1;
    int ret = 0;
    upgrade_buf = (unsigned char *)malloc(FILE_BUFFER);
    if(NULL == upgrade_buf) {
        db_msg("open upgrade_buf malloc fail\n");
        return -1;
    }
    memset(upgrade_buf,0,len);
    db_msg("*****%s,%d****",__FUNCTION__,__LINE__);
    if(!FILE_EXSIST(UPDATE_FILE_PATH)){
        db_msg("error, %s not exist", UPDATE_FILE_PATH);
        free(upgrade_buf);
        return -1;

    }
#ifdef FATFS
    int fp = fat_open(UPDATE_FILE_PATH, FA_READ);
#else
    int fp = open(UPDATE_FILE_PATH,O_RDONLY);
#endif
    if(fp < 0) {
        db_msg("open fp fail,erro=%s\n",strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    db_msg("*****%s,%d****\n",__FUNCTION__,__LINE__);
#ifdef FATFS
    //int ret = fat_read(fp,upgrade_buf,len);
    unsigned char * tmp = upgrade_buf;
    while(1) {
        ret = fat_read(fp,tmp,FILE_BUFFER);
        //db_msg("*****%d***ret=%d*\n",__LINE__,ret);
        if (ret == FAT_BUFFER_SIZE)
        {
            tmp += FAT_BUFFER_SIZE;
            continue;
        } else if (ret < 0) {
            db_msg ("Reading error, ret %d", ret);
            fat_close(fp);
            free(upgrade_buf);
            return -1;
        }
        break;
    }
#else
    ret = read(fp,upgrade_buf,len);
#endif
    db_msg("--------%d***ret=%d*\n",__LINE__,ret);

#ifndef FATFS
    if(ret <= 0){
        close(fp);
        free(upgrade_buf);
        return -1;
    }
#endif

    tmp_buf = upgrade_buf;
    tmp_buf = tmp_buf+4;
    db_msg("[debug_jaosn]:THIS IS FOR TEST line:%d",__LINE__);
    db_msg("[debug_jaosn]:the boot0 magic len is :%d\n",strlen(BOOT0_MAIGC));

    for(unsigned int j = 0; j < strlen(BOOT0_MAIGC); j++)
    {

        if(*tmp_buf == *destTem)
        {
                printf("%c",*tmp_buf);
                tmp_buf++;
                destTem++;
        }else
        {
                //printf("[debug_jaosn]:this is a Illegal full_img.fex file\n");
                //printf("[debug_jaosn]:tmp_buf= %c;destTem=%c\n",*tmp_buf,*destTem);
                //update_flag = 0;
                //break;
        }
    }

#ifdef FATFS
    fat_close(fp);
#else
    close(fp);
#endif
    if(update_flag != 1){
        db_msg("***full_img.fex error!***");
        //return -1;//for v40 boot0 magic check failed.
    }

    db_msg("*****%s,%d****\n",__FUNCTION__,__LINE__);

    flash_handle = open("/dev/mtdblock0",O_RDWR);
    if(flash_handle < 0) {
        db_msg("open flash_handle fail\n");
        free(upgrade_buf);
        return -1;
    }
    ret_len = write(flash_handle,upgrade_buf,UBOOT_LEN);
    db_msg("line=%d,ret_len=%d,;len=%d",__LINE__,ret_len,UBOOT_LEN);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != UBOOT_LEN){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }

    offset = UBOOT_LEN;
    db_msg(" upgrade uboot waiting...,ret_len = 0x%x\n",ret_len);
    db_msg("*****%s,%d****",__FUNCTION__,__LINE__);
    flash_handle = open("/dev/mtdblock1",O_RDWR);
    if(flash_handle < 0) {
        db_msg("line=%d,open flash_handle fail erro=%s",__LINE__,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    ret_len = write(flash_handle,upgrade_buf + offset,part[0]);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != part[0]){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;     
    }

    db_msg(" upgrade kernel waiting...,ret_len = %d\n",ret_len);
    db_msg("*****%s,%d****",__FUNCTION__,__LINE__);

    offset += part[0];
    flash_handle = open("/dev/mtdblock2",O_RDWR);
    if(flash_handle < 0) {
        db_msg("line=%d,open flash_handle fail erro=%s",__LINE__,strerror(errno));
        free(upgrade_buf);
        return -1;
    }

    ret_len = write(flash_handle,upgrade_buf + offset,part[1]);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != part[1]){
        db_msg("ROOTFS_SYSTEM_LEN=%d,erro=%s",ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    db_msg("upgrade rootfs waiting...,ret_len = %d\n",ret_len);
    db_msg("*****%s,%d****",__FUNCTION__,__LINE__);

    offset += part[1] ;
    flash_handle = open("/dev/mtdblock3",O_RDWR );
    if(flash_handle < 0){
        db_msg("ROOTFS_SYSTEM_LEN=%d,erro=%s",ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    ret_len = write(flash_handle,upgrade_buf+offset,part[2]);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != part[2]){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        return -1;
    }
    db_msg("*****%s,%d*ret_len=%d***",__FUNCTION__,__LINE__,ret_len);
    db_msg("upgrade cfg waiting,ret_len %d\n",ret_len);

    offset += part[2] ;
    flash_handle = open("/dev/mtdblock4",O_RDWR);
    if(flash_handle < 0){
        db_msg("line=%d,erro=%s",__LINE__,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    ret_len = write(flash_handle,upgrade_buf+offset,part[3]);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != part[3]){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    db_msg("*****%s,%d*ret_len=%d***",__FUNCTION__,__LINE__,ret_len);
    db_msg(" upgrade bootlogo waiting ...,ret_len = %d\n",ret_len);

    offset += part[3] ;
    flash_handle = open("/dev/mtdblock5",O_RDWR);
    if(flash_handle < 0){
        db_msg("line=%d,erro=%s",__LINE__,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    ret_len = write(flash_handle,upgrade_buf+offset,part[4]);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != part[4]){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    db_msg("*****%s,%d*ret_len=%d***",__FUNCTION__,__LINE__,ret_len);
    db_msg("test upgrade BOOTLOGO_LEN,ret_len = %d\n",ret_len);

    offset += part[4];

    flash_handle = open("/dev/mtdblock6",O_RDWR);
    if(flash_handle < 0){
        db_msg("line=%d,erro=%s",__LINE__,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    ret_len = write(flash_handle,upgrade_buf+offset,part[5]);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != part[5]){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }

    db_msg("*****%s,%d****",__FUNCTION__,__LINE__);
#if  0 // ndef tiger_cdr
    offset += ENV_LEN ;
    flash_handle = open("/dev/mtdblock7",O_RDWR);
    if(flash_handle < 0){
        db_msg("line=%d,erro=%s",__LINE__,strerror(errno));
        free(upgrade_buf);
        return -1;
    }

    ret_len = write(flash_handle,upgrade_buf+offset,PRIVATE_LEN);
    sync();
    close(flash_handle);
    if(ret_len != PRIVATE_LEN){
        db_msg("line=%d,ret_len=%d,erro=%s",__LINE__,ret_len,strerror(errno));
        free(upgrade_buf);
        return -1;
    }
    db_msg("test upgrade PRIVATE_LEN,ret_len = %d\n",ret_len);
#endif
    free(upgrade_buf);
    return 0;
}




static void *otaUpdateThread(void *ptx)
{
    db_msg("******in otaupdate*****");
//  watchDogRun();
    //char ip_buf[20];
    //int i = -2;
    //int ip =0,ip2;
    //char ipaddr[32];
    //char gateway[32];
    //uint32_t prefixLength;
    //char dns1[32];
    //char dns2[32];
    //char server[32];
    uint32_t lease;
    //char vendorInfo[32];
    int ret = 0;
    //int len = 0x500000;

    //ret = sd_upgrade(FULL_IMAGE, NULL, FILE_BUFFER, (int *)ptx);
    if (ret < 0) {
        db_msg("error upgrade fail exit");
        SendMessage(MainWnd,MSG_UPDATE_OVER,0,0);
        kill(getpid(),SIGKILL);
    }
    //db_msg(" 20160729 waiting *****-----------------****");
    sleep(2);
    db_msg("****%d**out otaupdate*****",__LINE__);
    //remove(UPDATE_FILE_PATH);
    SendMessage(MainWnd,MSG_UPDATE_OVER,1,0);
//  kill(getpid(),SIGKILL);
    return NULL;
}




extern int ota_main(char *image_path, ota_update_part_flg ota_partition_flag);
static void *otaUpdatePkgThread(void *ptx)
{
    int ret = 0;
    db_msg("******in otaupdate*****\n");
//  watchDogRun();
    ota_update_part_flg ota_part_flg;
    memset(&ota_part_flg, 0, sizeof(ota_update_part_flg));
    ota_part_flg.boot_logo_flag = 1;
    ota_part_flg.data_flag      = 1;
    ota_part_flg.env_flag       = 1;
    ota_part_flg.kernel_flag    = 1;
    ota_part_flg.misc_flag      = 1;
    ota_part_flg.rootfs_flag    = 1;
    ota_part_flg.uboot_flag     = 1;
    ota_part_flg.nor_flag       = 1;//nor flash
    ota_part_flg.nand_flag      = 1;//nor flash

	if(access("/dev/nanda",0)==0)  //nand flash
	{
    	if(ota_part_flg.nand_flag == 1){
        	ret = ota_main((char *)UPDATE_FILE_PATH_NAND, ota_part_flg);
    	}
    }
	else if(access("/dev/mtdblock1",0)==0)  //nor flash
	{
	
    	if(ota_part_flg.nor_flag  == 1){
        	ret = ota_main((char *)UPDATE_FILE_PATH, ota_part_flg);
    	}
	}
	else
	{
		
	}
    if (ret < 0) {
        db_msg("error upgrade fail exit\n");
        SendMessage(MainWnd,MSG_UPDATE_OVER,0,0);
        kill(getpid(),SIGKILL);
    }
    db_msg(" finish OTA, waiting reboot*****-----------------****\n");
    sleep(10);
    db_msg("****%d**out otaupdate*****\n",__LINE__);
    SendMessage(MainWnd,MSG_UPDATE_OVER,1,0);
//  kill(getpid(),SIGKILL);
    return NULL;
}


OtaUpdate::OtaUpdate(HWND hwnd){
    mHwnd = hwnd;
}

static int readPartitionSize_v40()
{
    char buf[8]={0};
    //char *p1, *p2;

    FILE *fp = NULL;
    if((fp=fopen("/proc/cmdline", "r"))){
        char text[10*1024]={0};
        fread(text, sizeof(text), 1, fp);
    //  db_msg("text:%s", text);
    }else{
        db_msg("open /proc/cmdline failed!");
        fclose(fp);
        return -1;
    }
    if(fp){
        fclose(fp);
    }

    //read boot.fex
    //p1= (char *)"boot";
    //p1 = strstr(text, "boot");
    //p2 = strstr(p1, ":");
    //p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    //strncpy(buf, (p2+1), (p3-p2-1));
    //mPartSize[0] = atoi(buf);
    mPartSize[0] = 10752;//v40 cmdline not ready
    mPartSize[0]*=512;

    //read system.fex
    //p1= (char *)"rootfs";
    //p1 = strstr(text, "rootfs");
    //p2 = strstr(p1, ":");
    //p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    //strncpy(buf, (p2+1), (p3-p2-1));
    //mPartSize[1] = atoi(buf);
    mPartSize[1] = 18432;//v40 cmdline not ready
    mPartSize[1]*=512;


    //read boot_logo.fex
    //p1= (char *)"boot_logo";
    //p1 = strstr(text, "boot_logo");
    //p2 = strstr(p1, ":");
    //p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    //strncpy(buf, (p2+1), (p3-p2-1));
    //mPartSize[2] = atoi(buf);
    mPartSize[2] = 512;//v40 cmdline not ready
    mPartSize[2]*=512;

    //read  env.fex
    //p1= (char *)"env";
    //p1 = strstr(text, "env");
    //p2 = strstr(p1, ":");
    //p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    //strncpy(buf, (p2+1), (p3-p2-1));
    //mPartSize[3] = atoi(buf);
    mPartSize[3] = 256;//v40 cmdline not ready
    mPartSize[3]*=512;

    //read  private.fex
    //p1= (char *)"data";
    //p1 = strstr(text, "data");
    //p2 = strstr(p1, ":");
    //p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    //strncpy(buf, (p2+1), (p3-p2-1));
    //mPartSize[4] = atoi(buf);
    mPartSize[4] = 1024;//v40 cmdline not ready
    mPartSize[4]*=512;

    //read  private.fex
    //p1= (char *)"misc";
    //p1 = strstr(text, "misc");
    //p2 = strstr(p1, ":");
    //p3 = strstr(p2, "]");
    memset(buf, 0, sizeof(buf));
    //strncpy(buf, (p2+1), (p3-p2-1));
    //mPartSize[5] = atoi(buf);
    mPartSize[5] = 256;//v40 cmdline not ready
    mPartSize[5]*=512;

    int i;
    for(i=0; i<6; i++){
        db_msg("mPartSize[%d]:%d  ", i, mPartSize[i]);
    }

    return 0;

}



static int readPartitionSize_v3()
{
    char buf[8]={0}, text[10*1024]={0};
    char *p1,*p2,*p3;

    FILE *fp = NULL;
    if((fp=fopen("/proc/cmdline", "r"))){
        fread(text, sizeof(text), 1, fp);
    //  db_msg("text:%s", text);
    }else{
        db_msg("open /proc/cmdline failed!");
        fclose(fp);
        return -1;
    }
    if(fp){
        fclose(fp);
    }

    //read boot.fex
    //p1= (char *)"boot";
    p1 = strstr(text, "boot");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[0] = atoi(buf);
    mPartSize[0]*=512;

    //read system.fex
    //p1= (char *)"system";
    p1 = strstr(text, "system");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[1] = atoi(buf);
    mPartSize[1]*=512;

    //read cfg.fex 
    //p1= (char *)"cfg";
    p1 = strstr(text, "cfg");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[2] = atoi(buf);
    mPartSize[2]*=512;

    //read boot_logo.fex
    //p1= (char *)"boot_logo";
    p1 = strstr(text, "boot_logo");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[3] = atoi(buf);
    mPartSize[3]*=512;

    //read  shutdown_logo.fex
    //p1= (char *)"shutdown_logo";
    p1 = strstr(text, "shutdown_logo");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[4] = atoi(buf);
    mPartSize[4]*=512;

    //read  env.fex
    //p1= (char *)"env";
    p1 = strstr(text, "env");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[5] = atoi(buf);
    mPartSize[5]*=512;

    //read  private.fex
    //p1= (char *)"private";
    p1 = strstr(text, "private");
    p2 = strstr(p1, ":");
    p3 = strstr(p2, "]");
    //memset(buf, 0, sizeof(buf));
    strncpy(buf, (p2+1), (p3-p2-1));
    mPartSize[6] = atoi(buf);
    mPartSize[6]*=512;

    int i;
    for(i=0; i<7; i++){
        db_msg("mPartSize[%d]:%d  \n", i, mPartSize[i]);
    }

    return 0;

}

int OtaUpdate::startUpdate()
{
    int ret ;
    ret = readPartitionSize_v40();
    if(ret == -1){
        SendMessage(MainWnd,MSG_UPDATE_OVER,0,0);
        db_msg("read flash partition size failed\n");
        return -1;
    }
    MainWnd = mHwnd;
    pthread_t thread_id = 0;
    int err = pthread_create(&thread_id, NULL, otaUpdateThread, mPartSize);
    if(err || !thread_id) {
        db_msg("Create OtaUpdate  thread error.");
        return -1;
    }
    ALOGV("Create OtaUpdate thread success.");
    return 0;
}

int OtaUpdate::startUpdatePkg()
{
    MainWnd = mHwnd;
    pthread_t thread_id = 0;
    int err = pthread_create(&thread_id, NULL, otaUpdatePkgThread, mPartSize);
    if(err || !thread_id) {
        db_msg("Create OtaUpdate  thread error.");
        return -1;
    }
    ALOGV("Create OtaUpdate thread success.");
    return 0;
}
