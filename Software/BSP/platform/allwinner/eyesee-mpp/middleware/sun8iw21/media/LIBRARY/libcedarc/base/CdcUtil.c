
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdcSysinfo.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/


#include "cdc_log.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include "CdcUtil.h"


int ComputeLbcParameter(ve_lbc_in_param* in_param, ve_lbc_out_param* out_param)
{
    unsigned int is_lossy  = 1;
	unsigned int seg_rc_en = 1;
    unsigned int lbc_align = 256;
    unsigned int seg_w     = 16;
    unsigned int seg_h     = 8;
    unsigned int bitdepth  = 8;
    unsigned int cmp_ratio = 683; //*1x:1024; 1.2x:854; 1.5x: 683; 2x:512; 2.5x: 410
	unsigned int frm_w_align = ALIGN_XXB(seg_w, in_param->nWidht);
	//When ref LBC compressing, it also fills the upper and lower borders with 4 lines
	//16aligh,then plus 4,final 8aligh
    unsigned int frm_h_align = ALIGN_XXB(8, ALIGN_XXB(16, in_param->nHeight)+4);
	unsigned int seg_tar_bits = 0;
	unsigned int segline_tar_bits = 0;
	unsigned int seg_num = frm_w_align/seg_w;
    unsigned int buffer_size = 0;
    unsigned int bs_buffer_size = 0;
    unsigned int info_buffer_size = 0;

	if(in_param->eLbcMode == LBC_MODE_DISABLE)
		return 0;
	else if(in_param->eLbcMode == LBC_MODE_1_5X)//* 1.5x
		cmp_ratio = 683;
	else if(in_param->eLbcMode == LBC_MODE_2_0X)//* 2x
		cmp_ratio = 512;
	else if(in_param->eLbcMode == LBC_MODE_2_5X)//* 2.5x
		cmp_ratio = 410;
	else if(in_param->eLbcMode == LBC_MODE_NO_LOSSY)//* is no lossy
		is_lossy = 0;

	if(is_lossy == 0)
	{
		unsigned int c_busrt_num_lenth = 8;
		unsigned int align_c = 8;
		unsigned int mb_num = 6;
		unsigned int sb_num = 2;
		unsigned int lossless_add_bits = c_busrt_num_lenth + align_c  - 1 + 2*mb_num*sb_num + 8*mb_num*sb_num;
		seg_tar_bits = ((seg_w*seg_h*bitdepth*3/2 + lossless_add_bits + lbc_align - 1)/lbc_align)*lbc_align;
		segline_tar_bits = seg_num*seg_tar_bits;
	}
	else
	{
		seg_tar_bits = ((seg_w*seg_h*bitdepth*cmp_ratio*3/2/1024)/lbc_align)*lbc_align;
		if(seg_rc_en == 1)
		{
			segline_tar_bits = (frm_w_align*seg_h*bitdepth*3/2*cmp_ratio/1024);
			segline_tar_bits = ALIGN_XXB(lbc_align,segline_tar_bits);
		}
		else
		{
			segline_tar_bits = seg_num*seg_tar_bits;
		}
	}

    bs_buffer_size = segline_tar_bits* frm_h_align/seg_h;
    info_buffer_size =  (((frm_w_align / seg_w + 16 - 1) / 16) * 16 * (frm_h_align/ seg_h))*2;

    logv("info_buffer_size = %d, frm_w_align = %d, seg_w = %d, frm_h_align = %d, seg_h = %d",
        info_buffer_size, frm_w_align, seg_w, frm_h_align, seg_h);

    buffer_size= (bs_buffer_size+7) / 8 + info_buffer_size;

    int ext_size = 8*1024;
    buffer_size = buffer_size + ext_size;
    logv("ext_size = %d", ext_size);

    logv("buffer_size = %d, bs_buffer_size = %d, info_buffer_size = %d",
           buffer_size,  bs_buffer_size, info_buffer_size);

    out_param->buffer_size      = buffer_size;
    out_param->seg_tar_bits     = seg_tar_bits;
    out_param->segline_tar_bits = segline_tar_bits;
    out_param->is_lossy = is_lossy;
    out_param->seg_rc_en = seg_rc_en;

    logv("in: w&h = %d,%d, lbcMode = %d",
         frm_w_align, frm_h_align, in_param->eLbcMode);
    logv("out: bufSize = %d, seg_bits = 0x%x, segline_bits = 0x%x, is_lossy = %d, seg_rc_en = %d",
         out_param->buffer_size, out_param->seg_tar_bits, out_param->segline_tar_bits,
         out_param->is_lossy, out_param->seg_rc_en);

	return 0;
}

int procInfo_initMem(struct ve_channel_proc_info *proc_info_mgr)
{
    if(proc_info_mgr == NULL)
    {
        loge("param error");
        return -1;
    }

    if(proc_info_mgr->base_info_data == NULL)
    {
        proc_info_mgr->base_info_data = malloc(BASE_PROC_BUF_LEN);
        if(proc_info_mgr->base_info_data == NULL)
        {
            loge("malloc failed, size = %d", BASE_PROC_BUF_LEN);
            return -1;
        }
        proc_info_mgr->advance_info_data = malloc(BASE_PROC_BUF_LEN);
        if(proc_info_mgr->advance_info_data == NULL)
        {
            loge("malloc failed, size = %d", BASE_PROC_BUF_LEN);
            return -1;
        }
    }
    return 0;
}

void procInfo_DestroyMem(struct ve_channel_proc_info *proc_info_mgr)
{
    if(proc_info_mgr == NULL)
    {
        loge("param error");
        return -1;
    }

    if(proc_info_mgr->base_info_data)
    {
        free(proc_info_mgr->base_info_data);
        proc_info_mgr->base_info_data = NULL;
    }

    if(proc_info_mgr->advance_info_data)
    {
        free(proc_info_mgr->advance_info_data);
        proc_info_mgr->advance_info_data = NULL;
    }
}

