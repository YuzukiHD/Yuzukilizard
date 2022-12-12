#include "imgdecode.h"
#include "sunxi_mbr.h"
#include "ota_private.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
//#include "app_log.h"


#define db_msg printf

#define  IMAGE_BUFFER_LENGTH_NOR     (16 * 1024 * 1024)
#define  IMAGE_BUFFER_LENGTH_NAND    (90 * 1024 * 1024)

#define  BOOT_LENGTH                 (512 * 1024)
#define  BOOT0_OFFSET                 0
#define  UBOOT_OFFSET                (24 * 1024)
#define  MBR_OFFSET                  (496 * 1024)
#define  BLKBURNBOOT0                _IO('v', 127)
#define  BLKBURNBOOT1                _IO('v', 128)


static void *imghd = NULL;
static void *imgitemhd = NULL;
static int  flash_handle = 0;

typedef enum {
    UBOOT_NOR,
    BOOT_NOR,
    ROOTFS_NOR,
    BOOTLOGO_NOR,
    ENV_NOR,
    DATA_NOR,
    MISC_NOR,
}flash_partition_nor_enum;

typedef enum {
    UBOOT_NAND,
    BOOT_NAND,
    ROOTFS_NAND,
    BOOTLOGO_NAND,
    ENV_NAND,
    PRIVATE_NAND,
    MISC_NAND,
}flash_partition_nand_enum;

int ota_open_firmware(char *name)
{
    db_msg("firmware name %s\n", name);
    imghd = Img_Open(name);
    if(!imghd)
    {
        return -1;
    }
    return 0;
}

int ota_get_partition_info(sunxi_download_info  *dl_map)
{
    imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890DLINFO");
    if(!imgitemhd)
    {
        return -1;
    }
    db_msg("try to read item dl map\n");
    if(!Img_ReadItem(imghd, imgitemhd, (void *)dl_map, sizeof(sunxi_download_info)))
    {
        db_msg("error : read dl map failed\n");
        return -1;
    }
    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;
    return 0;
}

int ota_read_mbr(void  *img_mbr, ota_update_part_flg ota_partition_flag)
{
    int mbr_num = SUNXI_MBR_COPY_NUM;

    if (ota_partition_flag.nor_flag == 1)
    {
       mbr_num = 1;
    }

    imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890___MBR");
    if(!imgitemhd)
    {
        return -1;
    }
    db_msg("try to read item dl map\n");
    if(!Img_ReadItem(imghd, imgitemhd, img_mbr, sizeof(sunxi_mbr_t) * mbr_num))
    {
        db_msg("error : read mbr failed\n");
        return -1;
    }
    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;
    return 0;
}

int ota_write_mbr(void  *img_mbr, unsigned int mbr_size, ota_update_part_flg ota_partition_flag)
{
    int ret_len = 0;

    /*write mbr infomation to flash*/
    flash_handle = open("/dev/mtdblock0",O_RDWR);
    if(flash_handle < 0) {
        db_msg("open flash_handle fail\n");
        return -1;
    }
    lseek(flash_handle, MBR_OFFSET, SEEK_SET);
    ret_len = write(flash_handle,img_mbr,mbr_size);
    db_msg("line=%d,ret_len=%d,;mbr_size=%ud \n",__LINE__,ret_len,mbr_size);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != mbr_size){
        db_msg("error! ota write mbr to flash failed! line=%d,ret_len=%d \n",__LINE__,ret_len);
        return -1;
    }
    return 0;
}

static int __download_normal_part(dl_one_part_info *part_info,  char *buffer, flash_partition_nor_enum part_num)
{
    int ret_len = 0;
    signed long long  partdata_by_byte;
    int  ret = -1;
    if((buffer == NULL)||(part_info == NULL)){

        db_msg("error, NULL pointer...\n");
        return -1;
    }

    /*open item by filename*/
    imgitemhd = Img_OpenItem(imghd, "RFSFAT16", (char *)part_info->dl_filename);
    if(!imgitemhd)
    {
        db_msg("error: ota open image item failed, file name: %s \n", part_info->dl_filename);
        return -1;
    }

    /*get file size*/
    partdata_by_byte = Img_GetItemSize(imghd, imgitemhd);
    if (partdata_by_byte <= 0)
    {
        db_msg("error: ota get item partition size failed, file name: %s \n", part_info->dl_filename);
        goto __download_normal_part_err1;
    }
    db_msg("partdata hi 0x%x\n", (uint)(partdata_by_byte>>32));
    db_msg("partdata lo 0x%x\n", (uint)partdata_by_byte);

    /*get file data to buffer*/
    Img_ReadItem(imghd, imgitemhd, buffer,IMAGE_BUFFER_LENGTH_NOR);

    /*app write file data to flash*/
    switch(part_num) {
        case UBOOT_NOR:
            break;
        case BOOT_NOR:
            {
                flash_handle = open("/dev/mtdblock1",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock1 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case ROOTFS_NOR:
            {
                flash_handle = open("/dev/mtdblock2",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock2 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case BOOTLOGO_NOR:
            {
                flash_handle = open("/dev/mtdblock3",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock3 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case ENV_NOR:
            {
                flash_handle = open("/dev/mtdblock4",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock4 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case DATA_NOR:
            {
                flash_handle = open("/dev/mtdblock5",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock5 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case MISC_NOR:
            {
                flash_handle = open("/dev/mtdblock6",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock6 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        default:
            db_msg("error, open flash partition number invalid\n");
            break;
    }

    db_msg("write partition to flash,waiting......\n");
    db_msg("ota write normal partition to flash, line=%d, partdata_by_byte=%lld \n",__LINE__,partdata_by_byte);
    ret_len = write(flash_handle,buffer,partdata_by_byte);
    db_msg("ota write normal partition to flash, line=%d,ret_len=%d,;partdata_by_byte=%lld \n",__LINE__,ret_len,partdata_by_byte);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != partdata_by_byte){
        db_msg("error! ota write flash failed. line=%d,ret_len=%d \n",__LINE__,ret_len);
        ret = -1;
        goto __download_normal_part_err1;
    }
    db_msg("successed in writting a file to flash partition, file name is: %s\n", part_info->name);
    ret = 0;

__download_normal_part_err1:
    if(imgitemhd)
    {
        Img_CloseItem(imghd, imgitemhd);
        imgitemhd = NULL;
    }
    return ret;
}

static int __download_normal_part_nand(dl_one_part_info *part_info,  char *buffer, flash_partition_nand_enum part_num)
{
    int ret_len = 0;
    signed long long  partdata_by_byte;
    int  ret = -1;
    if((buffer == NULL)||(part_info == NULL)){

        db_msg("error, NULL pointer...\n");
        return -1;
    }

    /*open item by filename*/
    imgitemhd = Img_OpenItem(imghd, "RFSFAT16", (char *)part_info->dl_filename);
    if(!imgitemhd)
    {
        db_msg("error: ota open image item failed, file name: %s \n", part_info->dl_filename);
        return -1;
    }

    /*get file size*/
    partdata_by_byte = Img_GetItemSize(imghd, imgitemhd);
    if (partdata_by_byte <= 0)
    {
        db_msg("error: ota get item partition size failed, file name: %s \n", part_info->dl_filename);
        goto __download_normal_part_err1;
    }
    db_msg("partdata hi 0x%x\n", (uint)(partdata_by_byte>>32));
    db_msg("partdata lo 0x%x\n", (uint)partdata_by_byte);

    /*get file data to buffer*/
    Img_ReadItem(imghd, imgitemhd, buffer,IMAGE_BUFFER_LENGTH_NAND);

    /*app write file data to flash*/
    switch(part_num) {
        case UBOOT_NAND:
            break;
        case BOOT_NAND:
            {
                flash_handle = open("/dev/nandc",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock1 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case ROOTFS_NAND:
            {
                flash_handle = open("/dev/nandd",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock2 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case BOOTLOGO_NAND:
            {
                flash_handle = open("/dev/nanda",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock3 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case ENV_NAND:
            {
                flash_handle = open("/dev/nandb",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock4 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case PRIVATE_NAND:
            {
                flash_handle = open("/dev/nande",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock5 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        case MISC_NAND:
            {
                flash_handle = open("/dev/nandf",O_RDWR);
                if(flash_handle < 0) {
                    db_msg("open mtdblock6 fail\n");
                    ret = -1;
                    goto __download_normal_part_err1;
                }
            }
            break;

        default:
            db_msg("error, open flash partition number invalid\n");
            break;
    }

    db_msg("write partition to flash,waiting......\n");
    ret_len = write(flash_handle,buffer,partdata_by_byte);
    db_msg("ota write normal partition to flash, line=%d,ret_len=%d,;partdata_by_byte=%lld \n",__LINE__,ret_len,partdata_by_byte);
    fdatasync(flash_handle);
    close(flash_handle);
    if(ret_len != partdata_by_byte){
        db_msg("error! ota write flash failed. line=%d,ret_len=%d \n",__LINE__,ret_len);
        ret = -1;
        goto __download_normal_part_err1;
    }
    db_msg("successed in writting a file to flash partition, file name is: %s\n", part_info->name);
    ret = 0;

__download_normal_part_err1:
    if(imgitemhd)
    {
        Img_CloseItem(imghd, imgitemhd);
        imgitemhd = NULL;
    }
    return ret;
}

int ota_update_normal_part(sunxi_download_info *dl_map, ota_update_part_flg ota_partition_flag)
{
    dl_one_part_info    *part_info;
    int                 ret  = -1;
    int                 ret1;
    int                   i  = 0;
    char *buffer         = NULL;
    buffer = (char *)malloc(IMAGE_BUFFER_LENGTH_NOR);      //16M, v40 nor
    if(buffer == NULL){

        db_msg("error, malloc buffer failed...\n");
        return -1;
    }
    memset(buffer, 0x00, IMAGE_BUFFER_LENGTH_NOR);         //memset(buffer, 0x00, IMAGE_BUFFER_LENGTH);
    if(!dl_map->download_count)
    {
        db_msg("ota update normal partition error: no part need to write\n");
        goto __ota_update_normal_part_err2;
    }

    /*ota update kernel*/
    if(ota_partition_flag.kernel_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("boot", (char *)part_info->name)){
                ret1 = __download_normal_part(part_info, buffer, BOOT_NOR);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update rootfs*/
    if(ota_partition_flag.rootfs_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("rootfs", (char *)part_info->name)){
                ret1 = __download_normal_part(part_info, buffer, ROOTFS_NOR);
                if(ret1 != 0)
                {
                    db_msg("err: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update boot logo*/
    if(ota_partition_flag.boot_logo_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("boot_logo", (char *)part_info->name)){
                ret1 = __download_normal_part(part_info, buffer, BOOTLOGO_NOR);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update env*/
    if(ota_partition_flag.env_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("env", (char *)part_info->name)){
                ret1 = __download_normal_part(part_info, buffer, ENV_NOR);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update data*/
    if(ota_partition_flag.data_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("data", (char *)part_info->name)){
                ret1 = __download_normal_part(part_info, buffer, DATA_NOR);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update misc*/
    if(ota_partition_flag.misc_flag== 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("misc", (char *)part_info->name)){
                ret1 = __download_normal_part(part_info, buffer, MISC_NOR);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }
    ret = 0;
__ota_update_normal_part_err2:
    if(buffer)
    {
        free(buffer);
    }
    return ret;
}



int ota_update_normal_part_nand(sunxi_download_info *dl_map, ota_update_part_flg ota_partition_flag)
{
    dl_one_part_info    *part_info;
    int                 ret  = -1;
    int                 ret1;
    int                   i  = 0;
    char *buffer         = NULL;
    buffer = (char *)malloc(IMAGE_BUFFER_LENGTH_NAND);//16M for v40 nor, 30M for v40 nand
    if(buffer == NULL){

        db_msg("error, malloc buffer failed...\n");
        return -1;
    }
    memset(buffer, 0x00, IMAGE_BUFFER_LENGTH_NAND);          //memset(buffer, 0x00, IMAGE_BUFFER_LENGTH);  nand

    if(!dl_map->download_count)
    {
        db_msg("ota update normal partition error: no part need to write\n");
        goto __ota_update_normal_part_err2;
    }


    /*ota update kernel*/
    if(ota_partition_flag.kernel_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("boot", (char *)part_info->name)){
                ret1 = __download_normal_part_nand(part_info, buffer, BOOT_NAND);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update rootfs*/
    if(ota_partition_flag.rootfs_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("rootfs", (char *)part_info->name)){
                ret1 = __download_normal_part_nand(part_info, buffer, ROOTFS_NAND);
                if(ret1 != 0)
                {
                    db_msg("err: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update boot logo*/
    if(ota_partition_flag.boot_logo_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("boot_logo", (char *)part_info->name)){
                ret1 = __download_normal_part_nand(part_info, buffer, BOOTLOGO_NAND);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update env*/
    if(ota_partition_flag.env_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("env", (char *)part_info->name)){
                ret1 = __download_normal_part_nand(part_info, buffer, ENV_NAND);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update data*/
    if(ota_partition_flag.private_flag == 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("private", (char *)part_info->name)){
                ret1 = __download_normal_part_nand(part_info, buffer, PRIVATE_NAND);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }

    /*ota update misc*/
    if(ota_partition_flag.misc_flag== 1){
        for(part_info = dl_map->one_part_info, i = 0; i < dl_map->download_count; i++, part_info++)
        {
            if (!strcmp("misc", (char *)part_info->name)){
                ret1 = __download_normal_part_nand(part_info, buffer, MISC_NAND);
                if(ret1 != 0)
                {
                    db_msg("error: ota_update_normal_part, download normal failed\n");
                    //goto __ota_update_normal_part_err2;
                }else{
                    db_msg("successed in download part %s\n", part_info->name);
                }
            }
        }
    }
    ret = 0;
__ota_update_normal_part_err2:
    if(buffer)
    {
        free(buffer);
    }
    return ret;
}

int ota_update_uboot(ota_update_part_flg ota_partition_flag)
{
    int ret = 0;
    int ret_len = 0;
    char buffer[4 * 1024 * 1024] = {0};
    uint item_original_size;

    /*{filename = "boot_package_nor.fex", maintype = "12345678",    subtype = "BOOTPKG-NOR00000",},*/
    imgitemhd = Img_OpenItem(imghd, "12345678", "BOOTPKG-NOR00000");

    if(!imgitemhd)
    {
        db_msg("sprite update error: fail to open uboot item\n");
        return -1;
    }
    /*get uboot size*/
    item_original_size = Img_GetItemSize(imghd, imgitemhd);
    db_msg("ota get nor uboot item_original_size: %ud \n", item_original_size);
    if(!item_original_size)
    {
        db_msg("sprite update error: fail to get uboot item size\n");
        ret = -1;
        goto __uboot_err1;
    }

    /*get uboot data*/

    if(!Img_ReadItem(imghd, imgitemhd, (void *)buffer, item_original_size))
    {
        db_msg("update error: fail to read data from for uboot\n");
        ret = -1;
        goto __uboot_err1;
    }

    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;


#if 0 //test
#define UBOOT_FILE_PATH_NOR         "/mnt/extsd/u-boot-spinor.fex"

        int fp = open(UBOOT_FILE_PATH_NOR,O_RDONLY);
        ret = read(fp,buffer,4*1024*1024);
        db_msg("--------%d***UBOOT ret=%d*\n",__LINE__,ret);
#endif

    /*app_write_flash*/
    if(ota_partition_flag.nor_flag == 1){
        flash_handle = open("/dev/mtdblock0",O_RDWR);
        if(flash_handle < 0) {
            db_msg("open flash_handle fail\n");
            return -1;
        }
        lseek(flash_handle, UBOOT_OFFSET, SEEK_SET);
        ret_len = write(flash_handle,buffer,item_original_size);

        db_msg("line=%d,ret_len=%d,;item_original_size=%ud",__LINE__,ret_len,item_original_size);
        fdatasync(flash_handle);
        close(flash_handle);
        if(ret_len != item_original_size){
            db_msg("error! ota write uboot to flash failed! line=%d,ret_len=%d \n",__LINE__,ret_len);
            return -1;
        }
    }

    db_msg("ota_update_uboot ok\n");
    return 0;
__uboot_err1:
    if(imgitemhd)
    {
        Img_CloseItem(imghd, imgitemhd);
        imgitemhd = NULL;
    }
    return ret;
}

int ota_update_uboot_nand(ota_update_part_flg ota_partition_flag)
{
    int ret = 0;
    int ret_len = 0;
    char buffer[4 * 1024 * 1024] = {0};
    uint item_original_size;
    int nand_fd = 0;
    imgitemhd = Img_OpenItem(imghd, "12345678", "BOOTPKG-00000000");

    if(!imgitemhd)
    {
        db_msg("sprite update error: fail to open uboot item\n");
        return -1;
    }

    /*get uboot size*/
    item_original_size = Img_GetItemSize(imghd, imgitemhd);
    db_msg("ota get nand boot1 item_original_size: %ud \n", item_original_size);
    if(!item_original_size)
    {
        db_msg("sprite update error: fail to get uboot item size\n");
        ret = -1;
        goto __uboot_err1;
    }

    /*get uboot data*/
    if(!Img_ReadItem(imghd, imgitemhd, (void *)buffer, item_original_size))
    {
        db_msg("update error: fail to read data from for uboot\n");
        ret = -1;
        goto __uboot_err1;
    }

    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;

#if 0 //test
//#define UBOOT_FILE_PATH_NAND          "/mnt/extsd/u-boot.fex"
#define UBOOT_FILE_PATH_NAND            "/home/boot_package.fex"


        int fp = open(UBOOT_FILE_PATH_NAND,O_RDONLY);
        ret = read(fp,buffer,4*1024*1024);
        db_msg("--------%d***UBOOT ret=%d*\n",__LINE__,ret);
#endif

    /*write_nand_flash*/
    nand_fd = open("/dev/nanda", O_RDWR);
    if (nand_fd < 0)
    {
        db_msg("Failed to open nanda device \n");
        return  -1;
    }
    int args[2]={0};
    args[0] = (int)buffer;
    args[1] = item_original_size;
    ioctl(nand_fd, BLKBURNBOOT1, args);

    db_msg("ota_update_uboot nand ok\n");
    close(nand_fd);
    return 0;
__uboot_err1:
    if(imgitemhd)
    {
        Img_CloseItem(imghd, imgitemhd);
        imgitemhd = NULL;
    }
    return ret;
}

int ota_update_boot0(ota_update_part_flg ota_partition_flag)
{
    int ret_len = 0;
    int ret = 0;
    char buffer[3*1024*1024] = {0};
    uint item_original_size;

    /*{filename = "boot0_spinor.fex", maintype = "12345678",        subtype = "1234567890BNOR_0",},*/
    if(ota_partition_flag.nor_flag == 1){
            db_msg("opening boot0 item for nor, 1234567890BNOR_0.....\n");
            imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890BNOR_0");
    }

    if(!imgitemhd)
    {
        db_msg("sprite update error: fail to open boot0 item\n");
        return -1;
    }
    /*get boot0 size*/
    item_original_size = Img_GetItemSize(imghd, imgitemhd);
    db_msg("ota get nor boot0 item_original_size: %ud \n", item_original_size);
    if(!item_original_size)
    {
        db_msg("sprite update error: fail to get boot0 item size\n");
        ret = -1;
        goto __boot0_err1;
    }

    /* get boot0 data*/


    //if(!Img_ReadItem(imghd, imgitemhd, (void *)buffer, 1 * 1024 * 1024))
    if(!Img_ReadItem(imghd, imgitemhd, (void *)buffer, item_original_size))
    {
        db_msg("update error: fail to read data from for boot0\n");
        ret = -1;
        goto __boot0_err1;
    }


#if 0
#define BOOT0_FILE_PATH_NOR         "/mnt/extsd/boot0_spinor.fex"

    int fp = open(BOOT0_FILE_PATH_NOR,O_RDONLY);
    ret = read(fp,buffer,3*1024*1024);
    db_msg("--------%d***boot0 ret=%d*\n",__LINE__,ret);
#endif

    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;

/*app_write_nor_flash*/
    if(ota_partition_flag.nor_flag == 1){
        flash_handle = open("/dev/mtdblock0",O_RDWR);
        if(flash_handle < 0) {
            db_msg("open flash_handle fail\n");
            return -1;
        }

        ret_len = write(flash_handle,buffer,item_original_size);
        db_msg("line=%d,ret_len=%d,;boot0_original_size=%ud \n",__LINE__,ret_len,item_original_size);
        fdatasync(flash_handle);
        close(flash_handle);
        if(ret_len != item_original_size){
            db_msg("error! ota write boot0 to flash failed! line=%d,ret_len=%d \n",__LINE__,ret_len);
            return -1;
        }
    }

    db_msg("ota_update_boot0 ok\n");
    return 0;
__boot0_err1:
    if(imgitemhd)
    {
        Img_CloseItem(imghd, imgitemhd);
        imgitemhd = NULL;
    }
    return ret;
}


int ota_update_boot0_nand(ota_update_part_flg ota_partition_flag)
{
    int ret_len = 0;
    int ret = 0;
    char buffer[3*1024*1024] = {0};
    uint item_original_size;
    int     nand_fd = 0;

    if(ota_partition_flag.nand_flag == 1){
            db_msg("opening boot0 item for nand, BOOT0_0000000000.....\n");
            imgitemhd = Img_OpenItem(imghd, "BOOT    ", "BOOT0_0000000000");
    }

    if(!imgitemhd)
    {
        db_msg("sprite update error: fail to open boot0 item\n");
        return -1;
    }
    /*get boot0 size*/
    item_original_size = Img_GetItemSize(imghd, imgitemhd);
    db_msg("ota get nand boot0 item_original_size: %ud \n", item_original_size);
    if(!item_original_size)
    {
        db_msg("sprite update error: fail to get boot0 item size\n");
        ret = -1;
        goto __boot0_err1;
    }

    /* get boot0 data*/
    if(!Img_ReadItem(imghd, imgitemhd, (void *)buffer, item_original_size))
    {
        db_msg("update error: fail to read data from for boot0\n");
        ret = -1;
        goto __boot0_err1;
    }

#if 0//test
//#define BOOT0_FILE_PATH_NAND          "/mnt/extsd/boot0_nand.fex"
#define BOOT0_FILE_PATH_NAND            "/home/boot0_nand.fex"

    int fp = open(BOOT0_FILE_PATH_NAND,O_RDONLY);
    ret = read(fp,buffer,3*1024*1024);
    db_msg("--------%d***boot0 ret=%d*\n",__LINE__,ret);
#endif

    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;

    /*write_nand_flash*/
    nand_fd = open("/dev/nanda", O_RDWR);
    if (nand_fd < 0)
    {
        db_msg("Failed to open nanda device \n");
        return  -1;
    }
    int args[2]={0};
    args[0] = (int)buffer;
    args[1] = item_original_size;
    ioctl(nand_fd, BLKBURNBOOT0, args);

    db_msg("ota_update_boot0 nand ok\n");
    close(nand_fd);
    return 0;
__boot0_err1:
    if(imgitemhd)
    {
        Img_CloseItem(imghd, imgitemhd);
        imgitemhd = NULL;
    }
    return ret;
}
