
/*
 ******************************************************************************
 *
 * isp.h
 *
 * Hawkview ISP - isp.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/05/27	VIDEO INPUT
 *
 *****************************************************************************
 */
#ifndef _ISP_H_
#define _ISP_H_
#include "include/isp_type.h"
#include "include/isp_manage.h"
#include "include/isp_tuning.h"
#include "isp_version.h"
int media_dev_init(void);
void media_dev_exit(void);
int isp_ir_reset(int dev_id, int mode_flag);
int isp_read_cfg_bin(int dev_id, int mode_flag, char *isp_cfg_bin_path);
int isp_reset(int dev_id);
int isp_init(int dev_id);
int isp_update(int dev_id);
int isp_get_imageparams(int dev_id, isp_image_params_t *pParams);
int isp_stop(int dev_id);
int isp_stop_ldci(int dev_id);
int isp_exit(int dev_id);
int isp_run(int dev_id);
int isp_events_stop(int dev_id);
int isp_events_init(int dev_id);
HW_S32 isp_pthread_join(int dev_id);
HW_S32 isp_get_cfg(int dev_id, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data);
HW_S32 isp_set_cfg(int dev_id, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data);
HW_S32 isp_stats_req(int dev_id, struct isp_stats_context *stats_ctx);

HW_S32 isp_set_fps(int dev_id, int sensor_fps);
int isp_set_ae_roi(int dev_id, struct isp_h3a_coor_win *coor, HW_U16 force_ae_target, HW_U16 enable);
HW_S32 isp_set_attr_cfg(int dev_id, HW_U32 ctrl_id, void *value);
HW_S32 isp_get_attr_cfg(int dev_id, HW_U32 ctrl_id, void *value);
HW_S32 isp_set_saved_ctx(int dev_id);
HW_S32 isp_get_sensor_info(int dev_id, struct sensor_config *cfg);

HW_S32 isp_get_version(char* version);
HW_S32 isp_get_awb_gain_ir(int dev_id, HW_S32 *rgain_ir, HW_S32 *bgain_ir);
HW_S32 isp_get_ae_info(int dev_id, isp_3a_info_ae *ae_info);
HW_S32 isp_get_awb_info(int dev_id, isp_3a_info_awb *awb_info);
HW_S32 isp_set_ae_flicker_comp(int dev_id, HW_S16 enable);


/*******************isp for video buffer*********************/
int isp_get_temp(int dev_id);
HW_S32 isp_get_lv(int dev_id);
HW_S32 isp_get_ev_lv_adj(int dev_id);
HW_S32 isp_get_encpp_cfg(int dev_id, HW_U32 ctrl_id, void *value);
HW_S32 isp_get_debug_msg(int dev_id, void* msg);
HW_S32 isp_get_3a_parameters(int dev_id, void* params);
void* isp_get_ctx_addr(int dev_id);
HW_S32 isp_get_yuv_ystat(struct isp_size pic_size, struct isp_h3a_coor_win target_area, void* VirAddr);
HW_S32 isp_get_awb_stats_avg(int dev_id, HW_U32 *awb_stats_ravg, HW_U32 *awb_stats_gavg, HW_U32 *awb_stats_bavg);
HW_S32 isp_get_ae_weight_lum(int dev_id);

#endif /*_ISP_H_*/



