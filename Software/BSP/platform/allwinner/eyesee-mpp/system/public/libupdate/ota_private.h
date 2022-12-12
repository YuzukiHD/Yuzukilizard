/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __OTA_PRIVATE_H____
#define __OTA_PRIVATE_H____

#include "sunxi_mbr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_FILE_PATH            "/mnt/extsd/sun8iw12p1_linux_v5s_pro_lpddr3_uart0_limate.img"
#define UPDATE_FILE_PATH_NAND         "/home/sun8iw12p1_linux_dvb_uart0.img"
// #define UPDATE_FILE_PATH_NAND           "/mnt/udisk/sun8iw12p1_linux_dvb_uart0.img"

typedef struct
{
    int uboot_flag;
    int kernel_flag;
    int rootfs_flag;
    int boot_logo_flag;
    int env_flag;
    int data_flag;
    int private_flag;
    int misc_flag;
    int nor_flag;
    int nand_flag;
}ota_update_part_flg;

int ota_open_firmware(char *name);
int ota_get_partition_info(sunxi_download_info  *dl_map);

int ota_read_mbr(void  *img_mbr, ota_update_part_flg ota_partition_flag);
int ota_write_mbr(void  *img_mbr, unsigned int mbr_size, ota_update_part_flg ota_partition_flag);

int ota_update_normal_part(sunxi_download_info *dl_map, ota_update_part_flg ota_partition_flag);
int ota_update_normal_part_nand(sunxi_download_info *dl_map, ota_update_part_flg ota_partition_flag);
int ota_update_uboot(ota_update_part_flg ota_partition_flag);
int ota_update_uboot_nand(ota_update_part_flg ota_partition_flag);
int ota_update_boot0(ota_update_part_flg ota_partition_flag);
int ota_update_boot0_nand(ota_update_part_flg ota_partition_flag);

int ota_main(char *image_path, ota_update_part_flg ota_partition_flag);

#ifdef __cplusplus
}
#endif
#endif //__OTA_PRIVATE_H____
