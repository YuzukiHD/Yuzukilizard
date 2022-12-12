#include "ota_private.h"
#include "sunxi_mbr.h"
#include "imgdecode.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
//#include "app_log.h"


#define db_msg printf

void __dump_dlmap(sunxi_download_info *dl_info)
{
    dl_one_part_info        *part_info;
    u32 i;
    char buffer[32];

    db_msg("*************DOWNLOAD MAP DUMP************\n");
    db_msg("total download part %ud\n", dl_info->download_count);
    db_msg("\n");
    for(part_info = dl_info->one_part_info, i=0;i<dl_info->download_count;i++, part_info++)
    {
        memset(buffer, 0, 32);
        memcpy(buffer, part_info->name, 16);
        db_msg("download part[%ud] name          :%s\n", i, buffer);
        //memset(buffer, 0, 32);
        memcpy(buffer, part_info->dl_filename, 16);
        db_msg("download part[%ud] download file :%s\n", i, buffer);
        //memset(buffer, 0, 32);
        memcpy(buffer, part_info->vf_filename, 16);
        db_msg("download part[%ud] verify file   :%s\n", i, buffer);
        db_msg("download part[%ud] lenlo         :0x%x\n", i, part_info->lenlo);
        db_msg("download part[%ud] addrlo        :0x%x\n", i, part_info->addrlo);
        db_msg("download part[%ud] encrypt       :0x%x\n", i, part_info->encrypt);
        db_msg("download part[%ud] verify        :0x%x\n", i, part_info->verify);
        db_msg("\n");
    }
}


void __dump_mbr(sunxi_mbr_t *mbr_info)
{
    sunxi_partition         *part_info;
    u32 i;
    char buffer[32] = {0};

    db_msg("*************MBR DUMP***************\n");
    db_msg("total mbr part %ud\n", mbr_info->PartCount);
    db_msg("\n");
    for(part_info = mbr_info->array, i=0;i<mbr_info->PartCount;i++, part_info++)
    {
        memset(buffer, 0, 32);
        memcpy(buffer, part_info->name, 16);
        db_msg("part[%ud] name      :%s\n", i, buffer);
        //memset(buffer, 0, 32);
        memcpy(buffer, part_info->classname, 16);
        db_msg("part[%ud] classname :%s\n", i, buffer);
        db_msg("part[%ud] addrlo    :0x%x\n", i, part_info->addrlo);
        db_msg("part[%ud] lenlo     :0x%x\n", i, part_info->lenlo);
        db_msg("part[%ud] user_type :0x%x\n", i, part_info->user_type);
        db_msg("part[%ud] keydata   :0x%x\n", i, part_info->keydata);
        db_msg("part[%ud] ro        :0x%x\n", i, part_info->ro);
        db_msg("\n");
    }
}

static int ota_update_boot(ota_update_part_flg ota_partition_flag)
{
    uchar  img_mbr[1024 * 1024] = {0};          //mbr, inclued by uboot, has some usefull info, suas private part...
    //int mbr_num = SUNXI_MBR_COPY_NUM;           //for nor flash, should be 1.

	if(access("/dev/mtdblock1",0)==0)  //nor flash
	{
	    if(ota_partition_flag.nor_flag == 1){
#if 0
	        /*update nor flash boot0*/
	        if(ota_update_boot0(ota_partition_flag))
	        {
	            db_msg("error : download boot0 error\n");
	            //return -1;
	        }
	        db_msg("success ota update boot0...\n");
#endif

	        /*update nor flash uboot*/
	        if(ota_update_uboot(ota_partition_flag))
	        {
	            db_msg("error : download uboot error\n");
	            //return -1;
	        }
	        db_msg("success in downloading uboot\n");

	        /*updtae mbr data when nor flash. nand flash don't need update mbr*/
	        db_msg("ota fetch mbr \n");
	        if(ota_read_mbr(&img_mbr, ota_partition_flag))
	        {
	            db_msg("error : fetch mbr error \n");
	            //return -1;
	        }
	        int mbr_num = 1;
	        if(ota_write_mbr(&img_mbr, sizeof(sunxi_mbr_t) * mbr_num, ota_partition_flag))
	        {
	            db_msg("error: download mbr err \n");
	            return -1;
	        }
	        db_msg("success in downloading mbr \n");
	        //__dump_mbr((sunxi_mbr_t *)img_mbr);
    	}
	}
	else if(access("/dev/nanda",0)==0)	//nand flash
	{		
	    if(ota_partition_flag.nand_flag == 1)
	    {
	        /*update nand flash boot0*/
	        if(ota_update_boot0_nand(ota_partition_flag))
	        {
	            db_msg("error : download boot0 error\n");
	            //return -1;
	        }
	        db_msg("success ota update boot0...\n");

	        /*update nand flash uboot, or named boot1*/
	        if(ota_update_uboot_nand(ota_partition_flag))
	        {
	            db_msg("error : download uboot error\n");
	            //return -1;
	        }
	        db_msg("success in downloading uboot\n");

	        /*updtae mbr data when nor flash. nand don't need mbr*/
	    }
	}
	else          //other flash 
	{
		
	}
	
    return 0;
}

int ota_main(char *image_path, ota_update_part_flg ota_partition_flag)
{
    sunxi_download_info  dl_map;                     //dlinfo, including packing and flash parting message, will be used in ota
    memset(&dl_map, 0, sizeof(sunxi_download_info));
    db_msg("ota begin......\n");

    /*get firmware image handle*/
    if(ota_open_firmware(image_path))
    {
        db_msg("error, get image file failed!\n");

        return -1;
    }
    db_msg("get image file success\n");

    /*get dl map, dl_map has partition information, can be used to get item info*/
    db_msg("fetch download map\n");
    if(ota_get_partition_info(&dl_map))
    {
        db_msg("error : fetch download map error\n");
        //return -1;
    }
    __dump_dlmap(&dl_map);


	if(access("/dev/mtdblock1",0)==0)  //nor flash
	{
		db_msg("flash_type is nor\n");
	    /*ota update nor flash normal partiton*/
	    if(ota_partition_flag.nor_flag == 1){
	        db_msg("begin to download normal part to nor flash\n");
	        if(ota_update_normal_part(&dl_map, ota_partition_flag))
	        {
	            db_msg("error : download part error\n");
	            //return -1;
	        }
	        db_msg("successed in downloading normal partition to nor flash\n");
	    }
	}
	else if(access("/dev/nanda",0)==0)  //nand flash
	{
		db_msg("flash_type is nand\n");
	    /*ota update nand flash normal partiton*/
	    if(ota_partition_flag.nand_flag == 1){
	        db_msg("begin to download normal part to nand flash\n");
	        if(ota_update_normal_part_nand(&dl_map, ota_partition_flag))
	        {
	            db_msg("error : download part error\n");
	            //return -1;
	        }
	        db_msg("successed in downloading normal partition to nand flash\n");
	    }
	}
	else                 //other flash
	{	
		
	}
    /*update uboot for nor or nand flash*/
    if(ota_update_boot(ota_partition_flag))
    {
        db_msg("error : download uboot error\n");
        //return -1;
    }
    db_msg("successed in downloading uboot\n");

    db_msg("ota update success&&&&&&&&&&&&&&&&&&&&&&&&&&& \n");
    sleep(30);
    return 0;
}
